#include "tbx/ui/ui.h"
#include "tbx/debug/log.h"
#include "tbx/gfx/gpu.h"
#include "tbx/utils/hash.h"
#include <RmlUi/Core.h>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx::ui
{
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
                log_error("rmlui: {}", message);
            else if (type == Rml::Log::LT_WARNING)
                log_warn("rmlui: {}", message);
            else
                log_info("rmlui: {}", message);
            return true;
        }

      public:
        double elapsed = 0.0;
    };

    /// @brief
    /// Purpose: One compiled RmlUi geometry batch kept engine-side and replayed via
    /// gpu::draw_ui.
    struct UiGeometry
    {
        std::vector<gpu::UiVertex> vertices = {};
        std::vector<int> indices = {};
    };

    /// @brief
    /// Purpose: RmlUi's renderer, implemented entirely against the gpu boundary — no library
    /// types cross it, so the gfx backend can swap under the UI untouched.
    class RenderInterface final : public Rml::RenderInterface
    {
      public:
        Rml::CompiledGeometryHandle CompileGeometry(
            Rml::Span<const Rml::Vertex> vertices,
            Rml::Span<const int> indices) override
        {
            auto geometry = UiGeometry {};
            geometry.vertices.reserve(vertices.size());
            for (const Rml::Vertex& vertex : vertices)
            {
                auto converted = gpu::UiVertex {};
                converted.position = Vec2(vertex.position.x, vertex.position.y);
                std::memcpy(&converted.color, &vertex.colour, sizeof(converted.color));
                converted.uv = Vec2(vertex.tex_coord.x, vertex.tex_coord.y);
                geometry.vertices.push_back(converted);
            }
            geometry.indices.assign(indices.begin(), indices.end());
            const auto handle = static_cast<Rml::CompiledGeometryHandle>(_next_handle++);
            _geometry[handle] = std::move(geometry);
            return handle;
        }

        void RenderGeometry(
            Rml::CompiledGeometryHandle handle,
            Rml::Vector2f translation,
            Rml::TextureHandle texture) override
        {
            const auto geometry = _geometry.find(handle);
            if (geometry == _geometry.end())
                return;
            auto bound = std::optional<std::reference_wrapper<const gpu::Texture2d>> {};
            const auto found = _textures.find(texture);
            if (found != _textures.end())
                bound = *found->second;
            gpu::draw_ui(
                geometry->second.vertices,
                geometry->second.indices,
                bound,
                Vec2(translation.x, translation.y));
        }

        void ReleaseGeometry(Rml::CompiledGeometryHandle handle) override
        {
            _geometry.erase(handle);
        }

        Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String& source) override
        {
            log_warn("ui file texture '{}' not supported yet; use generated textures", source);
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

      private:
        std::unordered_map<Rml::CompiledGeometryHandle, UiGeometry> _geometry;
        std::unordered_map<Rml::TextureHandle, std::unique_ptr<gpu::Texture2d>> _textures;
        uint64 _next_handle = 1;
        bool _scissor_enabled = false;
    };

    /// @brief
    /// Purpose: One cached document instance: content-hashed, shown while drawn, closed after
    /// going undrawn for a while (a changed asset hashes to a fresh instance).
    struct DocumentEntry
    {
        Rml::ElementDocument* document = nullptr; // owned by its Rml context
        uint64 last_drawn_frame = 0;
    };

    /// @brief
    /// Purpose: A context rendering into an offscreen target, with its own document cache.
    struct OffscreenContext
    {
        Rml::Context* context = nullptr; // owned by Rml until Rml::Shutdown
        std::unordered_map<uint64, DocumentEntry> documents;
        int width = 0;
        int height = 0;
    };

    /// @brief
    /// Purpose: The whole UI stack, torn down by reset() and rebuilt lazily.
    struct UiState
    {
        SystemInterface system = {};
        RenderInterface renderer = {};
        Rml::Context* context = nullptr; // owned by Rml until Rml::Shutdown
        std::unordered_map<uint64, DocumentEntry> documents;
        std::unordered_map<uint32, OffscreenContext> offscreen_by_target;
        std::unordered_map<std::string, std::string> bindings;
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
            log_error("RmlUi initialization failed; ui disabled");
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
                log_warn("font '{}' failed to load", entry.path().string());
        }

        const int width = std::max(1, gpu::get_viewport_width());
        const int height = std::max(1, gpu::get_viewport_height());
        state->context = Rml::CreateContext("tbx", Rml::Vector2i(width, height));
        if (!state->context)
        {
            log_error("RmlUi context creation failed; ui disabled");
            return nullptr;
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

    //// DOCUMENT CACHE ////

    static DocumentEntry* ensure_document(
        Rml::Context& context,
        std::unordered_map<uint64, DocumentEntry>& documents,
        const UiDocument& document)
    {
        const uint64 key = hash(std::string_view(document.text));
        const auto found = documents.find(key);
        if (found != documents.end())
            return &found->second;
        Rml::ElementDocument* loaded = context.LoadDocumentFromMemory(document.text);
        if (!loaded)
        {
            log_error("ui document failed to parse");
            return nullptr;
        }
        auto& entry = documents[key];
        entry.document = loaded;
        return &entry;
    }

    /// @brief
    /// Purpose: Shows what was drawn, hides what was not, closes the long-undrawn.
    static void settle_documents(
        UiState& state,
        std::unordered_map<uint64, DocumentEntry>& documents)
    {
        for (auto it = documents.begin(); it != documents.end();)
        {
            DocumentEntry& entry = it->second;
            if (entry.last_drawn_frame == state.frame)
            {
                apply_bindings(state, entry.document);
                entry.document->Show();
                ++it;
            }
            else if (state.frame - entry.last_drawn_frame > UNDRAWN_FRAMES_BEFORE_CLOSE)
            {
                entry.document->Close();
                it = documents.erase(it);
            }
            else
            {
                entry.document->Hide();
                ++it;
            }
        }
    }

    //// BOUNDARY ////

    void draw(const UiDocument& document)
    {
        UiState* state = ensure_ui_ready();
        if (!state)
            return;
        if (DocumentEntry* entry = ensure_document(*state->context, state->documents, document))
            entry->last_drawn_frame = state->frame;
    }

    void draw(const UiDocument& document, const gpu::RenderTarget& target)
    {
        UiState* state = ensure_ui_ready();
        if (!state)
            return;
        auto& offscreen = state->offscreen_by_target[target.get_framebuffer()];
        if (!offscreen.context)
        {
            offscreen.context = Rml::CreateContext(
                std::format("tbx_target_{}", target.get_framebuffer()),
                Rml::Vector2i(target.get_width(), target.get_height()));
            if (!offscreen.context)
            {
                log_error("RmlUi offscreen context failed");
                state->offscreen_by_target.erase(target.get_framebuffer());
                return;
            }
        }
        if (offscreen.width != target.get_width() || offscreen.height != target.get_height())
        {
            offscreen.width = target.get_width();
            offscreen.height = target.get_height();
            offscreen.context->SetDimensions(Rml::Vector2i(offscreen.width, offscreen.height));
        }
        DocumentEntry* entry =
            ensure_document(*offscreen.context, offscreen.documents, document);
        if (!entry)
            return;
        entry->last_drawn_frame = state->frame;
        apply_bindings(*state, entry->document);
        entry->document->Show();
        offscreen.context->Update();

        gpu::begin_render_pass(
            {.color_target = target,
             .load = gpu::LoadOperation::CLEAR,
             .clear_color = Color {.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f}});
        offscreen.context->Render();
        gpu::set_scissor(false, 0, 0, 0, 0);
        gpu::end_render_pass();
    }

    void render()
    {
        if (!g_ui)
            return;
        UiState& state = *g_ui;
        settle_documents(state, state.documents);
        for (auto& [target, offscreen] : state.offscreen_by_target)
            settle_documents(state, offscreen.documents);
        state.context->Update();
        state.context->Render();
        gpu::set_scissor(false, 0, 0, 0, 0);
        ++state.frame;
    }

    void reset()
    {
        g_ui.reset();
    }

    void set_binding(const std::string& name, const std::string& value)
    {
        if (UiState* state = ensure_ui_ready())
            state->bindings[name] = value;
    }

    void set_binding(const std::string& name, const double value)
    {
        // std::format trims trailing zeros so "3" stays "3" while "2.5" stays "2.5".
        set_binding(name, std::format("{}", value));
    }

    void update(const float delta_time)
    {
        if (!g_ui)
            return;
        g_ui->system.elapsed += delta_time;
        g_ui->context->SetDimensions(
            Rml::Vector2i(gpu::get_viewport_width(), gpu::get_viewport_height()));
    }
}
