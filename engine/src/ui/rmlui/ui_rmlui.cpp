#include "tbx/ui/ui.h"
#include "tbx/debug/log.h"
#include "tbx/files/files.h"
#include "tbx/gfx/gpu.h"
#include "tbx/utils/hash.h"
#include <RmlUi/Core.h>
#include <charconv>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
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
    /// ordinary gpu::Mesh (position2 + color4 + uv2 floats, indices expanded), drawn with the
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
            _meshes[handle] = gpu::upload_mesh(floats, std::array {2, 4, 2});
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
            gpu::set_uniform(*shader, "u_translation", Vec2(translation.x, translation.y));
            const auto found = _textures.find(texture);
            const gpu::Texture2d& bound =
                found != _textures.end() ? *found->second : *white_texture;
            const auto bindings =
                std::array {gpu::TextureBinding {.slot = 0, .texture = std::cref(bound)}};
            gpu::draw(*mesh->second, bindings);
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
            auto uploaded = gpu::upload_texture(
                dimensions.x,
                dimensions.y,
                std::span<const std::byte>(
                    reinterpret_cast<const std::byte*>(source.data()), source.size()));
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
                gpu::set_scissor(false, 0, 0, 0, 0);
        }

        void SetScissorRegion(Rml::Rectanglei region) override
        {
            if (_scissor_enabled)
                gpu::set_scissor(
                    true, region.Left(), region.Top(), region.Width(), region.Height());
        }

      public:
        // Set by draw_to() around context rendering.
        const gpu::Shader* shader = nullptr; // raw at the library boundary, like the contexts
        const gpu::Texture2d* white_texture = nullptr;

      private:
        std::unordered_map<Rml::CompiledGeometryHandle, std::unique_ptr<gpu::Mesh>> _meshes;
        std::unordered_map<Rml::TextureHandle, std::unique_ptr<gpu::Texture2d>> _textures;
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
    /// Purpose: The whole UI stack, torn down by reset() and rebuilt lazily.
    struct UiState
    {
        SystemInterface system = {};
        RenderInterface renderer = {};
        std::unordered_map<uint64, DocumentEntry> documents; // keyed by content hash ^ target
        std::unordered_map<std::string, std::string> bindings; // slot -> latest value
        std::unordered_map<std::string, UiBinding> live_bindings; // evaluated every update
        /// @brief
        /// Purpose: One queued document plus its (possibly custom) shader stage sources.
        struct QueuedDocument
        {
            UiDocument document = {};
            std::string vertex_source = {};
            std::string fragment_source = {};
        };

        /// @brief
        /// Purpose: A compiled ui shader pair and its premultiplied pipeline, cached by the
        /// stage sources' hash (key 0 = the builtin ui shaders).
        struct UiPipeline
        {
            std::unique_ptr<gpu::Shader> shader;
            std::unique_ptr<gpu::Pipeline> pipeline;
        };

        std::vector<QueuedDocument> queued; // what draw() collected for the next draw_to()
        std::unordered_map<uint64, UiPipeline> pipelines;
        std::string builtin_vertex_text;
        std::string builtin_fragment_text;
        std::unique_ptr<gpu::Texture2d> white;
        uint64 frame = 1;
        bool is_initialized = false;

        ~UiState()
        {
            if (is_initialized)
                Rml::Shutdown();
        }
    };

    static std::unique_ptr<UiState> g_ui = {};
    static constexpr uint64 UNDRAWN_FRAMES_BEFORE_CLOSE = 600;

    static UiState* ensure_ui_ready()
    {
        if (g_ui)
            return g_ui.get();
        auto state = std::make_unique<UiState>();
        Rml::SetSystemInterface(&state->system);
        Rml::SetRenderInterface(&state->renderer);
        if (!Rml::Initialise())
        {
            TBX_ERROR("RmlUi initialization failed; ui disabled");
            return nullptr;
        }
        state->is_initialized = true;

        // Every face in the engine's font folder registers as a fallback-capable family.
        const auto fonts = std::filesystem::path(TBX_RESOURCES_PATH) / "Fonts";
        auto ec = std::error_code {};
        for (const auto& entry : std::filesystem::directory_iterator(fonts, ec))
        {
            const auto extension = entry.path().extension().string();
            if (extension != ".ttf" && extension != ".otf")
                continue;
            if (!Rml::LoadFontFace(entry.path().string(), true))
                TBX_WARN("font '{}' failed to load", entry.path().string());
        }
        g_ui = std::move(state);
        return g_ui.get();
    }

    //// BINDINGS ////

    /// @brief
    /// Purpose: Pushes binding values into one element tree: data-text fills inner text,
    /// data-style replaces the style attribute. Applied values cache on the element so
    /// unchanged bindings never force relayout.
    static void apply_bindings(UiState& state, Rml::Element* element)
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
        for (const char* property : {"width", "height"})
        {
            const auto attribute = std::format("data-{}", property);
            const Rml::Variant* size_binding = element->GetAttribute(attribute);
            if (!size_binding)
                continue;
            const auto found = state.bindings.find(size_binding->Get<Rml::String>());
            if (found == state.bindings.end())
                continue;
            auto scale = 1.0f;
            if (const Rml::Variant* scale_attribute =
                    element->GetAttribute(attribute + "-scale"))
                scale = scale_attribute->Get<float>();
            auto value = 0.0f;
            const auto text = found->second;
            std::from_chars(text.data(), text.data() + text.size(), value);
            const auto pixels = std::format("{}px", value * scale);
            const auto applied_key = std::format("data-applied-{}", property);
            const Rml::Variant* applied = element->GetAttribute(applied_key);
            if (!applied || applied->Get<Rml::String>() != pixels)
            {
                element->SetProperty(property, pixels);
                element->SetAttribute(applied_key, pixels);
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
        UiState& state,
        const UiDocument& document,
        const uint64 key,
        const int width,
        const int height)
    {
        auto& entry = state.documents[key];
        if (!entry.context)
        {
            entry.context = Rml::CreateContext(
                std::format("tbx_{}", key), Rml::Vector2i(width, height));
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
            entry.document = entry.context->LoadDocumentFromMemory(document.text);
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

    static bool ensure_builtin_ui_shaders(UiState& state)
    {
        if (!state.builtin_vertex_text.empty())
            return true;
        const auto shaders = std::filesystem::path(TBX_RESOURCES_PATH) / "Shaders" / "Tbx";
        const auto vertex = files::read_text(shaders / "ui.vert");
        const auto fragment = files::read_text(shaders / "ui.frag");
        if (!vertex || !fragment)
        {
            TBX_ERROR("ui shaders missing under resources/Shaders/Tbx");
            return false;
        }
        state.builtin_vertex_text = *vertex;
        state.builtin_fragment_text = *fragment;
        constexpr std::byte WHITE[4] = {
            std::byte {255}, std::byte {255}, std::byte {255}, std::byte {255}};
        state.white = gpu::upload_texture(1, 1, WHITE);
        return true;
    }

    /// @brief
    /// Purpose: The pipeline for one document's shader stages: either stage may be custom
    /// source, the other falls back to the builtin ui stage; pairs cache together.
    static UiState::UiPipeline* resolve_ui_pipeline(
        UiState& state,
        const std::string& vertex_source,
        const std::string& fragment_source)
    {
        const uint64 key = (vertex_source.empty() && fragment_source.empty())
            ? 0
            : hash(std::string_view(vertex_source)) ^ ~hash(std::string_view(fragment_source));
        const auto found = state.pipelines.find(key);
        if (found != state.pipelines.end())
            return found->second.shader ? &found->second : nullptr;

        const std::string& vertex =
            vertex_source.empty() ? state.builtin_vertex_text : vertex_source;
        const std::string& fragment =
            fragment_source.empty() ? state.builtin_fragment_text : fragment_source;
        auto compiled = gpu::compile_shader(vertex.c_str(), fragment.c_str());
        if (!compiled)
        {
            TBX_ERROR("ui shader failed: {}", compiled.error());
            state.pipelines[key] = {}; // remember the failure; warn once
            return nullptr;
        }
        auto& entry = state.pipelines[key];
        entry.shader = std::move(*compiled);
        entry.pipeline = gpu::make_pipeline(
            {.shader = *entry.shader,
             .is_depth_test_enabled = false,
             .is_depth_write_enabled = false,
             .cull = gpu::CullMode::NONE,
             .blend = gpu::BlendMode::PREMULTIPLIED});
        return &entry;
    }

    //// BOUNDARY ////

    void draw(
        const UiDocument& document,
        const std::string_view vertex_shader,
        const std::string_view fragment_shader)
    {
        UiState* state = ensure_ui_ready();
        if (!state)
            return;
        state->queued.push_back(
            {.document = document,
             .vertex_source = std::string(vertex_shader),
             .fragment_source = std::string(fragment_shader)});
    }

    void draw_to(const gpu::RenderTarget& target)
    {
        UiState* state = ensure_ui_ready();
        if (!state)
            return;
        auto queued = std::move(state->queued);
        state->queued.clear();
        if (!ensure_builtin_ui_shaders(*state))
            return;

        gpu::begin_render_pass(
            {.color_target = target,
             .load = gpu::LoadOperation::CLEAR,
             .clear_color = Color {.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f}});
        const auto screen = Vec2(
            static_cast<float>(target.get_width()), static_cast<float>(target.get_height()));
        state->renderer.white_texture = state->white.get();
        for (const UiState::QueuedDocument& entry : queued)
        {
            UiState::UiPipeline* pipeline =
                resolve_ui_pipeline(*state, entry.vertex_source, entry.fragment_source);
            if (!pipeline)
                continue;
            gpu::set_pipeline(*pipeline->pipeline);
            gpu::set_uniform(*pipeline->shader, "u_texture", 0);
            gpu::set_uniform(*pipeline->shader, "u_screen", screen);
            gpu::set_uniform(
                *pipeline->shader, "u_time", static_cast<float>(state->system.elapsed));
            state->renderer.shader = pipeline->shader.get();

            const uint64 key = hash(std::string_view(entry.document.text));
            DocumentEntry* cached = ensure_document(
                *state, entry.document, key, target.get_width(), target.get_height());
            if (!cached)
                continue;
            cached->last_drawn_frame = state->frame;
            apply_bindings(*state, cached->document);
            cached->context->Update();
            cached->context->Render();
        }
        state->renderer.shader = nullptr;
        gpu::set_scissor(false, 0, 0, 0, 0);
        gpu::end_render_pass();
    }

    void reset()
    {
        g_ui.reset();
    }

    void bind(UiBinding binding)
    {
        if (UiState* state = ensure_ui_ready())
            state->live_bindings[binding.name] = std::move(binding);
    }

    void unbind(const std::string& name)
    {
        if (g_ui)
            g_ui->live_bindings.erase(name);
    }

    void set_string(const std::string& name, std::string value)
    {
        if (UiState* state = ensure_ui_ready())
            state->bindings[name] = std::move(value);
    }

    void set_bool(const std::string& name, const bool value)
    {
        set_string(name, value ? "true" : "false");
    }

    void set_color(const std::string& name, const Color& value)
    {
        set_string(
            name,
            std::format(
                "#{:02x}{:02x}{:02x}{:02x}",
                static_cast<int>(value.r * 255.0f),
                static_cast<int>(value.g * 255.0f),
                static_cast<int>(value.b * 255.0f),
                static_cast<int>(value.a * 255.0f)));
    }

    void set_float(const std::string& name, const float value)
    {
        set_string(name, std::format("{}", value));
    }

    void set_int(const std::string& name, const int value)
    {
        set_string(name, std::format("{}", value));
    }

    void set_vec2(const std::string& name, const Vec2& value)
    {
        set_string(name, std::format("{}, {}", value.x, value.y));
    }

    void set_vec3(const std::string& name, const Vec3& value)
    {
        set_string(name, std::format("{}, {}, {}", value.x, value.y, value.z));
    }

    void update(const float delta_time)
    {
        if (!g_ui)
            return;
        UiState& state = *g_ui;
        state.system.elapsed += delta_time;

        // Live bindings feed their slots once per frame; apply_bindings diffs per element.
        for (const auto& [name, binding] : state.live_bindings)
            if (binding.source)
                state.bindings[binding.name] = binding.source();

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
