#include "Tbx/App/PCH.h"
#include "Tbx/App/Renderer/RenderStack.h"
#include "Tbx/App/Plugins/PluginServer.h"
#include "Tbx/App/Renderer/RenderQueue.h"
#include <memory>

namespace Tbx
{
    std::shared_ptr<IRenderer> RenderStack::_renderer;
    std::weak_ptr<IWindow> RenderStack::_renderSurface;
    RenderQueue RenderStack::_renderQueue;
    bool RenderStack::_vsyncEnabled;

    void RenderStack::Initialize()
    {
        _renderer = PluginServer::GetPlugin<IRenderer>();
        TBX_VALIDATE_PTR(_renderer, "Failed to init rendering, because the renderer plugin failed to load or couldn't be found.");
    }

    void RenderStack::Shutdown()
    {
        Flush();

        _renderer.reset();
        _renderSurface.reset();
    }

    void RenderStack::SetVSyncEnabled(bool enabled)
    {
        _vsyncEnabled = enabled;

        _renderer->SetVSyncEnabled(enabled);
    }

    bool RenderStack::IsVSyncEnabled()
    {
        return _vsyncEnabled;
    }

    void RenderStack::Submit(const RenderCommand& command, const std::any& data)
    {
        if (_renderQueue.IsEmpty())
        {
            auto renderBatch = RenderBatch();
            renderBatch.AddItem({ command, data });
            _renderQueue.Push(renderBatch);
        }
        else
        {
            auto& frameBatch = _renderQueue.Peek();
            frameBatch.AddItem({ command, data });
        }
    }

    void RenderStack::Draw(const std::weak_ptr<IWindow>& surface)
    {
        TBX_VALIDATE_WEAK_PTR(surface, "Failed to draw to {0}, because the given weak pointer to the render surface is invalid.");

        if (_renderSurface.lock() != surface.lock())
        {
            // Update context if needed
            _renderer->SetContext(surface);
            _renderer->SetVSyncEnabled(_vsyncEnabled);
            _renderSurface = surface;
        }

        // Update viewport
        const auto& surfaceSize = surface.lock()->GetSize();
        _renderer->SetViewport({ 0, 0 }, surfaceSize);

        // Update camera
        const auto& surfaceView = surface.lock()->GetCamera().lock(); 
        surfaceView->SetAspect((float)surfaceSize.Width / (float)surfaceSize.Height);

        // Render whatever is in our queue
        ProcessNextBatch();
    }

    void RenderStack::Clear()
    {
        _renderer->Clear();
    }

    void RenderStack::Flush()
    {
        _renderer->Flush();
        _renderQueue.Clear();
    }

    void RenderStack::ProcessNextBatch()
    {
        _renderer->BeginDraw();

        if (!_renderQueue.IsEmpty())
        {
            const auto& batch = _renderQueue.Peek();

            for (const auto& item : batch)
            {
                using enum Tbx::RenderCommand;
                switch (item.Command)
                {
                    case Clear:
                    {
                        const auto& colorData = std::any_cast<Color>(item.Data);
                        _renderer->Clear(colorData);
                        break;
                    }
                    case UploadShader:
                    {
                        const auto& shaderData = std::any_cast<Shader>(item.Data);
                        _renderer->UploadShader(shaderData);
                        break;
                    }
                    case UploadTexture:
                    {
                        const auto& textureData = std::any_cast<TextureRenderData>(item.Data);
                        _renderer->UploadTexture(textureData.GetTexture(), textureData.GetSlot());
                        break;
                    }
                    case UploadShaderData:
                    {
                        const auto& shaderData = std::any_cast<ShaderData>(item.Data);
                        _renderer->UploadShaderData(shaderData);
                        break;
                    }
                    case RenderMesh:
                    {
                        const auto& meshData = std::any_cast<MeshRenderData>(item.Data);
                        _renderer->Draw(meshData.GetMesh(), meshData.GetMaterial());
                        break;
                    }
                    default:
                    {
                        TBX_ASSERT(false, "Unknown render command type.");
                        break;
                    }
                }
            }

            _renderQueue.Pop();
        }

        _renderer->EndDraw();
    }
}
