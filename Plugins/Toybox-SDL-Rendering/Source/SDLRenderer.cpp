#include "SDLRenderer.h"
#include <Tbx/Debug/Debugging.h>
#include <Tbx/Graphics/Material.h>
#include <Tbx/Graphics/Mesh.h>

namespace SDLRendering
{
    void SDLRenderer::Initialize(const std::shared_ptr<Tbx::IRenderSurface>& surface)
    {
        auto* window = (SDL_Window*)surface->GetNativeWindow();

        _renderer = std::shared_ptr<SDL_Renderer>(SDL_CreateRenderer(window, nullptr), [](SDL_Renderer* rendererToDelete) { SDL_DestroyRenderer(rendererToDelete); });
        _surface = surface;

        TBX_ASSERT(_surface, "No surface to render to was given!");
        TBX_ASSERT(_renderer, "Failed to create SDL_Renderer: {}", SDL_GetError());

        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        _resolution = { w, h };
        _viewport = { { 0, 0 }, { w, h } };

        SDL_SetRenderViewport(_renderer.get(), nullptr);

        std::string err = SDL_GetError();
        TBX_ASSERT(err.empty(), "An error from SDL has occured: {}", err);
    }

    Tbx::GraphicsDevice SDLRenderer::GetGraphicsDevice()
    {
        return _renderer.get();
    }

    void SDLRenderer::SetApi(Tbx::GraphicsApi api)
    {
        _api = api;
    }

    Tbx::GraphicsApi SDLRenderer::GetApi()
    {
        return _api;
    }

    void SDLRenderer::SetViewport(const Tbx::Viewport& viewport)
    {
        _viewport = viewport;
        SDL_Rect sdlViewport = { (int)viewport.Position.X, (int)viewport.Position.Y, (int)viewport.Size.Width, (int)viewport.Size.Height };
        SDL_SetRenderViewport(_renderer.get(), &sdlViewport);

        std::string err = SDL_GetError();
        TBX_ASSERT(err.empty(), "An error from SDL has occured: {}", err);
    }

    const Tbx::Viewport& SDLRenderer::GetViewport()
    {
        return _viewport;
    }

    void SDLRenderer::SetResolution(const Tbx::Size& size)
    {
        // TODO: Implement graphics resolution that is seperate from window resolution
        _resolution = size;
    }

    const Tbx::Size& SDLRenderer::GetResolution()
    {
        return _resolution;
    }

    void SDLRenderer::SetVSyncEnabled(bool enabled)
    {
        _vsyncEnabled = enabled;
        // TODO: Set vsync on SDL renderer
    }

    bool SDLRenderer::GetVSyncEnabled()
    {
        return _vsyncEnabled;
    }

    void SDLRenderer::Flush()
    {
        SDL_RenderPresent(_renderer.get());
    }

    void SDLRenderer::Clear(const Tbx::Color& color)
    {
        auto r = static_cast<Uint8>(color.R * 255);
        auto g = static_cast<Uint8>(color.G * 255);
        auto b = static_cast<Uint8>(color.B * 255);
        auto a = static_cast<Uint8>(color.A * 255);

        SDL_SetRenderDrawColor(_renderer.get(), r, g, b, a);
        SDL_RenderClear(_renderer.get());

        std::string err = SDL_GetError();
        TBX_ASSERT(err.empty(), "An error from SDL has occured: {}", err);
    }

    void SDLRenderer::Draw(const Tbx::FrameBuffer& buffer)
    {
        for (const auto& cmd : buffer.GetCommands())
        {
            switch (cmd.GetType())
            {
                case Tbx::DrawCommandType::Clear:
                {
                    const auto& color = std::any_cast<const Tbx::Color&>(cmd.GetPayload());
                    Clear(color);
                    break;
                }
                case Tbx::DrawCommandType::CompileMaterial:
                {
                    // Your system might load/compile shaders here
                    // You could use SDL_CreateShader() and SDL_CreatePipeline()
                    break;
                }
                case Tbx::DrawCommandType::SetMaterial:
                {
                    const auto& currentMaterial = std::any_cast<const Tbx::Material&>(cmd.GetPayload());
                    break;
                }
                case Tbx::DrawCommandType::UploadMaterialData:
                {
                    const auto& data = std::any_cast<const Tbx::ShaderData&>(cmd.GetPayload());
                    break;
                }
                case Tbx::DrawCommandType::DrawMesh:
                {
                    const auto& mesh = std::any_cast<const Tbx::Mesh&>(cmd.GetPayload());
                    break;
                }
                default:
                    break;
            }

            std::string err = SDL_GetError();
            TBX_ASSERT(err.empty(), "An error from SDL has occured: {}", err);
        }
    }
}
