#include "TbxPCH.h"
#include "Rendering.h"

#define TBX_VALIDATE_RENDERER(error_msg, ...)  if (_renderer == nullptr) { TBX_ERROR(error_msg, __VA_ARGS__); return; }

namespace Tbx
{
    std::shared_ptr<IRenderer> Rendering::_renderer;
    std::weak_ptr<IWindow> Rendering::_renderSurface;
    RenderQueue Rendering::_renderQueue;
    bool Rendering::_vsyncEnabled;

    void Rendering::Initialize()
    {
        _renderer = PluginServer::GetPlugin<IRenderer>();
    }

    void Rendering::Shutdown()
    {
        Flush();
        _renderer.reset();
        _renderSurface.reset();
    }

    void Rendering::SetVSyncEnabled(bool enabled)
    {
        _vsyncEnabled = enabled;

        TBX_VALIDATE_RENDERER("Failed to set vsync to {0}, because the renderer couldn't be found.", enabled);

        _renderer->SetVSyncEnabled(enabled);
    }

    bool Rendering::IsVSyncEnabled()
    {
        return _vsyncEnabled;
    }

    void Rendering::Submit(const RenderCommand& command, const std::any& data)
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

    void Rendering::Draw(const std::weak_ptr<IWindow>& surface)
    {
        TBX_VALIDATE_RENDERER("Failed to draw to {0}, because the renderer couldn't be found.", surface.lock()->GetTitle());

        if (!Tbx::IsWeakPointerValid(_renderSurface) || _renderSurface.lock() != surface.lock())
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

    TBX_API void Rendering::Clear()
    {
        TBX_VALIDATE_RENDERER("Failed to clear, because the renderer couldn't be found.");

        _renderer->Clear();
    }

    void Rendering::Flush()
    {
        _renderer->Flush();
        _renderQueue.Clear();
    }

    void Rendering::ProcessNextBatch()
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
                    _renderer->Clear();
                    break;
                }
                case SetShader:
                {
                    const auto& shaderData = std::any_cast<Shader>(item.Data);
                    _renderer->SetShader(shaderData);
                    break;
                }
                case UploadShaderData:
                {
                    const auto& shaderData = std::any_cast<ShaderData>(item.Data);
                    _renderer->UploadShaderData(shaderData);
                    break;
                }
                case RenderColor:
                {
                    const auto& colorData = std::any_cast<Color>(item.Data);
                    _renderer->Draw(colorData);
                    break;
                }
                case RenderTexture:
                {
                    const auto& textureData = std::any_cast<Texture>(item.Data);
                    _renderer->Draw(textureData);
                    break;
                }
                case RenderMesh:
                {
                    const auto& meshData = std::any_cast<Mesh>(item.Data);
                    _renderer->Draw(meshData);
                    break;
                }
                case RenderText:
                {
                    const auto& textData = std::any_cast<std::string>(item.Data);
                    _renderer->Draw(textData);
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
