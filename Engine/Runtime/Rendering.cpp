#include "TbxPCH.h"
#include "Rendering.h"

namespace Tbx
{
    std::shared_ptr<IRenderer> Rendering::_renderer;
    std::weak_ptr<IWindow> Rendering::_renderSurface;
    RenderQueue Rendering::_renderQueue;

    void Rendering::Initialize()
    {
        auto rendererFactory = ModuleServer::GetFactoryModule<IRenderer>();
        if (!Tbx::IsWeakPointerValid(rendererFactory))
        {
            TBX_ERROR("Failed to initialize rendering, because a renderer factory couldn't be found. Is a renderer module installed?");
        }
        else
        {
            const auto& sharedRenderer = rendererFactory.lock()->CreateShared();
            _renderer = sharedRenderer;
        }
    }

    TBX_API void Rendering::Shutdown()
    {
        Flush();
        _renderer.reset();
    }

    void Rendering::SetVSyncEnabled(bool enabled)
    {
        _renderer->SetVSyncEnabled(enabled);
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

    void Rendering::Draw(const std::weak_ptr<IWindow>& surface, bool resizing)
    {
        if (!Tbx::IsWeakPointerValid(_renderSurface) || _renderSurface.lock() != surface.lock())
        {
            // Update context
            _renderer->SetContext(surface);
            _renderer->SetVSyncEnabled(true); // Default to vsync on
            _renderSurface = surface;
        }

        // Update viewport
        const auto& surfaceSize = surface.lock()->GetSize();
        _renderer->SetViewport({ 0, 0 }, surfaceSize);

        _renderer->BeginDraw();

        // Process batch if we aren't resizing
        if (!resizing) ProcessNextBatch();
        
        _renderer->EndDraw();
    }

    void Rendering::Flush()
    {
        _renderer->Flush();
        _renderQueue.Clear();
    }

    void Rendering::ProcessNextBatch()
    {
        // TODO: we prolly don't want to constantly draw and flush if nothing has changed....
        while (!_renderQueue.IsEmpty())
        {
            const auto& batch = _renderQueue.Peek();

            for (const auto& item : batch)
            {
                using enum Tbx::RenderCommand;
                switch (item.Command)
                {
                case Clear:
                    _renderer->Clear(); break;
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
    }
}
