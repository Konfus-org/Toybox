#include "tbx/ui/ui.h"
#include "tbx/debug/log.h"
#include "tbx/files/files.h"
#include "tbx/gfx/gpu.h"
#include "tbx/utils/hash.h"
#include <RmlUi/Core.h>
#include <array>
#include <charconv>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx::ui
{
    // Style note: the PascalCase methods below (GetElapsedTime, CompileGeometry, ...) are
    // RmlUi's required virtual signatures — third-party shape, not ours. Everything we name
    // in this file follows the Toybox standard.

    /// @brief
    /// Purpose: RmlUi's clock (fed by update()) and log bridge.
    class SystemInterface final : public Rml::SystemInterface
    {
      public:
        double GetElapsedTime() override
        {
            return elapsed;
        }

        bool LogMessage(Rml::Log::Type type, const Rml::String& message) override
        {
            if (type <= Rml::Log::LT_ERROR)
                TBX_ERROR("rmlui: {}", message);
            else if (type == Rml::Log::LT_WARNING)
                TBX_WARN("rmlui: {}", message);
            else
                TBX_INFO("rmlui: {}", message);
            return true;
        }

      public:
        double elapsed = 0.0;
    };

    /// @brief
    /// Purpose: RmlUi's renderer over the generic gpu boundary: compiled geometry is an
    /// ordinary gfx::Mesh (position2 + color4 + uv2 floats, indices expanded), drawn with the
    /// engine ui shaders and per-draw texture bindings — no UI-specific gpu entry points.
    class RenderInterface final : public Rml::RenderInterface
    {
      public:
        Rml::CompiledGeometryHandle CompileGeometry(
            Rml::Span<const Rml::Vertex> vertices,
            Rml::Span<const int> indices) override
        {
            auto floats = std::vector<float>();
            floats.reserve(indices.size() * 8);
            for (const int index : indices)
            {
                const Rml::Vertex& vertex = vertices[static_cast<size>(index)];
                floats.push_back(vertex.position.x);
                floats.push_back(vertex.position.y);
                floats.push_back(vertex.colour.red / 255.0f);
                floats.push_back(vertex.colour.green / 255.0f);
                floats.push_back(vertex.colour.blue / 255.0f);
                floats.push_back(vertex.colour.alpha / 255.0f);
                floats.push_back(vertex.tex_coord.x);
                floats.push_back(vertex.tex_coord.y);
            }
            const auto handle = static_cast<Rml::CompiledGeometryHandle>(_next_handle++);
            _meshes[handle] = gfx::upload_mesh(floats, std::array {2, 4, 2});
            return handle;
        }

        void RenderGeometry(
            Rml::CompiledGeometryHandle handle,
            Rml::Vector2f translation,
            Rml::TextureHandle texture) override
        {
            const auto mesh = _meshes.find(handle);
            if (mesh == _meshes.end() || !shader)
                return;
            gfx::set_uniform(*shader, "u_translation", Vec2(translation.x, translation.y));
            const auto found = _textures.find(texture);
            const gfx::Texture2d& bound =
                found != _textures.end() ? *found->second : *white_texture;
            const auto bindings =
                std::array {gfx::TextureBinding {.slot = 0, .texture = std::cref(bound)}};
            gfx::draw(*mesh->second, bindings);
        }

        void ReleaseGeometry(Rml::CompiledGeometryHandle handle) override
        {
            _meshes.erase(handle);
        }

        Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String& source) override
        {
            TBX_WARN("ui file texture '{}' not supported yet; use generated textures", source);
            return {};
        }

        Rml::TextureHandle GenerateTexture(
            Rml::Span<const Rml::byte> source,
            Rml::Vector2i dimensions) override
        {
            auto uploaded = gfx::upload_texture(
                dimensions.x,
                dimensions.y,
                std::span<const std::byte>(
                    reinterpret_cast<const std::byte*>(source.data()),
                    source.size()));
            const auto handle = static_cast<Rml::TextureHandle>(_next_handle++);
            _textures[handle] = std::move(uploaded);
            return handle;
        }

        void ReleaseTexture(Rml::TextureHandle handle) override
        {
            _textures.erase(handle);
        }

        void EnableScissorRegion(bool enable) override
        {
            _scissor_enabled = enable;
            if (!enable)
                gfx::set_scissor(false, 0, 0, 0, 0);
        }

        void SetScissorRegion(Rml::Rectanglei region) override
        {
            if (_scissor_enabled)
                gfx::set_scissor(
                    true,
                    region.Left(),
                    region.Top(),
                    region.Width(),
                    region.Height());
        }

      public:
        // Set once when the pipeline compiles; non-owning views of UiState members with the
        // exact same lifetime (raw at the RmlUi library boundary, like the contexts).
        const gfx::Shader* shader = nullptr;
        const gfx::Texture2d* white_texture = nullptr;

      private:
        std::unordered_map<Rml::CompiledGeometryHandle, std::unique_ptr<gfx::Mesh>> _meshes;
        std::unordered_map<Rml::TextureHandle, std::unique_ptr<gfx::Texture2d>> _textures;
        uint64 _next_handle = 1;
        bool _scissor_enabled = false;
    };

    /// @brief
    /// Purpose: One drawn document: its own Rml context (so draw() can render it right now,
    /// alone), keyed by content hash — a changed asset hashes to a fresh instance.
    struct DocumentEntry
    {
        Rml::Context* context = nullptr; // owned by Rml until Rml::Shutdown
        Rml::ElementDocument* document = nullptr;
        uint64 last_drawn_frame = 0;
        int width = 0;
        int height = 0;
    };

    /// @brief
    /// Purpose: The document/render stack behind the boundary, built lazily on the first
    /// draw.
    struct UiState::Backend
    {
        SystemInterface system = {};
        RenderInterface renderer = {};
        std::unordered_map<uint64, DocumentEntry> documents; // keyed by content hash
        std::unique_ptr<gfx::Shader> shader; // the builtin ui raster shaders (files)
        std::unique_ptr<gfx::Pipeline> pipeline; // premultiplied, no depth
        std::unique_ptr<gfx::Texture2d> white;
        // Registered face bytes: RmlUi references memory faces until Rml::Shutdown, so the
        // boundary owns copies — the asset cache may drop its Font whenever it likes.
        std::vector<std::vector<std::byte>> font_faces;
        uint64 frame = 1;
        bool is_initialized = false;

        ~Backend()
        {
            if (is_initialized)
                Rml::Shutdown();
        }
    };

    UiState::UiState() = default;
    UiState::~UiState() = default;
    UiState::UiState(UiState&& other) noexcept = default;
    UiState& UiState::operator=(UiState&& other) noexcept = default;

    static constexpr uint64 UNDRAWN_FRAMES_BEFORE_CLOSE = 600;

    static std::optional<std::reference_wrapper<UiState::Backend>> ensure_ui_ready(
        UiState& ui_state)
    {
        if (ui_state.backend)
            return *ui_state.backend;
        auto state = std::make_unique<UiState::Backend>();
        Rml::SetSystemInterface(&state->system);
        Rml::SetRenderInterface(&state->renderer);
        if (!Rml::Initialise())
        {
            TBX_ERROR("RmlUi initialization failed; ui disabled");
            return {};
        }
        state->is_initialized = true;

        // Fonts are not this boundary's policy: faces arrive as Font assets via set_font()
        // (the runtime registers the engine's builtin face at boot).
        ui_state.backend = std::move(state);
        return *ui_state.backend;
    }

    //// BINDINGS ////

    /// @brief
    /// Purpose: Pushes binding values into one element tree: data-text fills inner text,
    /// data-style replaces the style attribute. Applied values cache on the element so
    /// unchanged bindings never force relayout.
    static void apply_bindings(const UiState& state, Rml::Element* element)
    {
        if (const Rml::Variant* text_binding = element->GetAttribute("data-text"))
        {
            const auto found = state.bindings.find(text_binding->Get<Rml::String>());
            if (found != state.bindings.end())
            {
                const Rml::Variant* applied = element->GetAttribute("data-applied-text");
                if (!applied || applied->Get<Rml::String>() != found->second)
                {
                    element->SetInnerRML(found->second);
                    element->SetAttribute("data-applied-text", found->second);
                }
            }
        }
        // Bars bind raw numbers: data-width="kills" data-width-scale="40" -> width in px.
        // Attribute names are fixed - spelled out once so this per-element, per-frame walk
        // never builds strings just to look them up.
        struct SizeAttribute
        {
            const char* property;
            const char* attribute;
            const char* scale_attribute;
            const char* applied_attribute;
        };
        static constexpr std::array<SizeAttribute, 2> SIZE_ATTRIBUTES = {
            SizeAttribute {"width", "data-width", "data-width-scale", "data-applied-width"},
            SizeAttribute {"height", "data-height", "data-height-scale", "data-applied-height"}};
        for (const SizeAttribute& names : SIZE_ATTRIBUTES)
        {
            const Rml::Variant* size_binding = element->GetAttribute(names.attribute);
            if (!size_binding)
                continue;
            const auto found = state.bindings.find(size_binding->Get<Rml::String>());
            if (found == state.bindings.end())
                continue;
            auto scale = 1.0f;
            if (const Rml::Variant* scale_attribute = element->GetAttribute(names.scale_attribute))
                scale = scale_attribute->Get<float>();
            auto value = 0.0f;
            const std::string& text = found->second;
            std::from_chars(text.data(), text.data() + text.size(), value);
            const auto pixels = std::format("{}px", value * scale);
            const Rml::Variant* applied = element->GetAttribute(names.applied_attribute);
            if (!applied || applied->Get<Rml::String>() != pixels)
            {
                element->SetProperty(names.property, pixels);
                element->SetAttribute(names.applied_attribute, pixels);
            }
        }
        if (const Rml::Variant* style_binding = element->GetAttribute("data-style"))
        {
            const auto found = state.bindings.find(style_binding->Get<Rml::String>());
            if (found != state.bindings.end())
            {
                const Rml::Variant* applied = element->GetAttribute("data-applied-style");
                if (!applied || applied->Get<Rml::String>() != found->second)
                {
                    element->SetAttribute("style", found->second);
                    element->SetAttribute("data-applied-style", found->second);
                }
            }
        }
        for (int child = 0; child < element->GetNumChildren(); ++child)
            apply_bindings(state, element->GetChild(child));
    }

    //// DRAW ////

    /// @brief
    /// Purpose: The cached (context, document) pair for one document at one drawable size,
    /// created on first draw.
    static DocumentEntry* ensure_document(
        UiState::Backend& state,
        const UiDocument& document,
        const uint64 key,
        const int width,
        const int height)
    {
        auto& entry = state.documents[key];
        if (!entry.context)
        {
            entry.context =
                Rml::CreateContext(std::format("tbx_{}", key), Rml::Vector2i(width, height));
            if (!entry.context)
            {
                TBX_ERROR("RmlUi context creation failed");
                state.documents.erase(key);
                return nullptr;
            }
            entry.width = width;
            entry.height = height;
        }
        if (entry.width != width || entry.height != height)
        {
            entry.width = width;
            entry.height = height;
            entry.context->SetDimensions(Rml::Vector2i(width, height));
        }
        if (!entry.document)
        {
            entry.document = entry.context->LoadDocumentFromMemory(document.source);
            if (!entry.document)
            {
                TBX_ERROR("ui document failed to parse");
                state.documents.erase(key);
                return nullptr;
            }
            entry.document->Show();
        }
        return &entry;
    }

    static bool ensure_ui_pipeline(UiState::Backend& state)
    {
        if (state.pipeline)
            return true;
        const auto shaders = std::filesystem::path(TBX_RESOURCES_PATH) / "Shaders" / "Tbx";
        const auto vertex = files::read_text(shaders / "ui.vert");
        const auto fragment = files::read_text(shaders / "ui.frag");
        if (!vertex || !fragment)
        {
            TBX_ERROR("ui shaders missing under resources/Shaders/Tbx");
            return false;
        }
        auto compiled = gfx::compile_shader(*vertex, *fragment);
        if (!compiled)
        {
            TBX_ERROR("ui shaders failed: {}", compiled.error());
            return false;
        }
        state.shader = std::move(*compiled);
        state.pipeline = gfx::make_pipeline(
            {.shader = *state.shader,
             .is_depth_test_enabled = false,
             .is_depth_write_enabled = false,
             .cull = gfx::CullMode::NONE,
             .blend = gfx::BlendMode::PREMULTIPLIED});
        constexpr std::byte white[4] = {
            std::byte {255},
            std::byte {255},
            std::byte {255},
            std::byte {255},
        };
        state.white = gfx::upload_texture(1, 1, white);
        // The render interface renders through these for the rest of the state's life.
        state.renderer.shader = state.shader.get();
        state.renderer.white_texture = state.white.get();
        return true;
    }

    //// BOUNDARY ////

    void draw(UiState& ui_state, const UiDocument& document, const gfx::RenderTarget& target)
    {
        const auto ready = ensure_ui_ready(ui_state);
        if (!ready)
            return;
        UiState::Backend* state = &ready->get();
        if (!ensure_ui_pipeline(*state))
            return;

        gfx::begin_render_pass(
            {.color_target = target,
             .load = gfx::LoadOperation::CLEAR,
             .clear_color = Color {.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f}});
        gfx::set_pipeline(*state->pipeline);
        gfx::set_uniform(*state->shader, "u_texture", 0);
        gfx::set_uniform(
            *state->shader,
            "u_screen",
            Vec2(static_cast<float>(target.get_width()), static_cast<float>(target.get_height())));
        const uint64 key = hash(std::string_view(document.source));
        if (DocumentEntry* cached =
                ensure_document(*state, document, key, target.get_width(), target.get_height()))
        {
            cached->last_drawn_frame = state->frame;
            apply_bindings(ui_state, cached->document);
            cached->context->Update();
            cached->context->Render();
        }
        gfx::set_scissor(false, 0, 0, 0, 0);
        gfx::end_render_pass();
    }

    void set_font(UiState& ui_state, const Font& font, const std::string& family)
    {
        const auto ready = ensure_ui_ready(ui_state);
        if (!ready)
            return;
        UiState::Backend* state = &ready->get();
        // The copy lands in the state first: RmlUi references the memory face until
        // Rml::Shutdown (which the state destructor runs before releasing these bytes).
        state->font_faces.push_back(font.data);
        const std::vector<std::byte>& data = state->font_faces.back();
        const auto span = Rml::Span<const Rml::byte>(
            reinterpret_cast<const Rml::byte*>(data.data()),
            data.size());
        if (!Rml::LoadFontFace(
                span,
                family,
                Rml::Style::FontStyle::Normal,
                Rml::Style::FontWeight::Auto,
                true))
        {
            TBX_WARN("font face '{}' failed to load", family);
            state->font_faces.pop_back();
        }
    }

    void bind(UiState& state, UiBinding binding)
    {
        state.live_bindings[binding.name] = std::move(binding);
    }

    void update(UiState& ui_state, const float delta_time)
    {
        // Live bindings feed their slots once per frame; apply_bindings diffs per element.
        for (const auto& [name, binding] : ui_state.live_bindings)
            if (binding.source)
                ui_state.bindings[binding.name] = binding.source();

        if (!ui_state.backend)
            return;
        UiState::Backend& state = *ui_state.backend;
        state.system.elapsed += delta_time;

        // What stopped being drawn retires; a changed asset simply hashes to a new entry.
        for (auto it = state.documents.begin(); it != state.documents.end();)
        {
            if (state.frame - it->second.last_drawn_frame > UNDRAWN_FRAMES_BEFORE_CLOSE)
            {
                it->second.document->Close();
                Rml::RemoveContext(it->second.context->GetName());
                it = state.documents.erase(it);
            }
            else
                ++it;
        }
        ++state.frame;
    }
}
