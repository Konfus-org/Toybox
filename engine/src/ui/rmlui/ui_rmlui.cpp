#include "tbx/ui/ui.h"
#include "tbx/core/log.h"
#include "tbx/gfx/gpu.h"
#include <RmlUi/Core.h>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tbx::ui
{
    /// @brief
    /// Purpose: RmlUi's clock, fed by update()'s delta times.
    class SystemInterface final : public Rml::SystemInterface
    {
      public:
        double GetElapsedTime() override
        {
            return elapsed;
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
    /// Purpose: Satisfies RmlUi's font-engine requirement while the "none" engine is selected
    /// (no text yet — a FreeType-backed engine slots in later without touching this boundary).
    class NullFontEngine final : public Rml::FontEngineInterface
    {
    };

    /// @brief
    /// Purpose: The whole UI stack, torn down by reset() and rebuilt lazily.
    struct UiState
    {
        SystemInterface system = {};
        RenderInterface renderer = {};
        NullFontEngine fonts = {};
        Rml::Context* context = nullptr; // owned by Rml until Rml::Shutdown
        std::unordered_map<uint64, Rml::ElementDocument*> documents;
        uint64 next_document_id = 1;
        bool is_initialized = false;

        ~UiState()
        {
            if (is_initialized)
                Rml::Shutdown();
        }
    };

    static std::unique_ptr<UiState> g_ui = {};

    static UiState* ensure_ui_ready()
    {
        if (g_ui)
            return g_ui.get();
        auto state = std::make_unique<UiState>();
        Rml::SetSystemInterface(&state->system);
        Rml::SetRenderInterface(&state->renderer);
        Rml::SetFontEngineInterface(&state->fonts);
        if (!Rml::Initialise())
        {
            log_error("RmlUi initialization failed; ui disabled");
            return nullptr;
        }
        state->is_initialized = true;
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

    //// BOUNDARY ////

    Result<uint64> load_document(const std::string& rml)
    {
        UiState* state = ensure_ui_ready();
        if (!state)
            return fail("ui is unavailable");
        Rml::ElementDocument* document = state->context->LoadDocumentFromMemory(rml);
        if (!document)
            return fail("document failed to parse");
        document->Show();
        const uint64 id = state->next_document_id++;
        state->documents[id] = document;
        return ok(id);
    }

    void render()
    {
        if (!g_ui)
            return;
        g_ui->context->Render();
        gpu::set_scissor(false, 0, 0, 0, 0);
    }

    void reset()
    {
        g_ui.reset();
    }

    void unload_document(const uint64 document_id)
    {
        if (!g_ui)
            return;
        const auto found = g_ui->documents.find(document_id);
        if (found == g_ui->documents.end())
            return;
        found->second->Close();
        g_ui->documents.erase(found);
    }

    void update(const float delta_time)
    {
        if (!g_ui)
            return;
        g_ui->system.elapsed += delta_time;
        g_ui->context->SetDimensions(
            Rml::Vector2i(gpu::get_viewport_width(), gpu::get_viewport_height()));
        g_ui->context->Update();
    }
}
