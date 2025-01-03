#include "Rendering.h"

namespace Tbx
{
    std::shared_ptr<IRenderer> Rendering::_renderer;
    std::weak_ptr<IWindow> Rendering::_lastSurface;
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
            auto sharedRenderer = rendererFactory.lock()->CreateShared();
            _renderer = sharedRenderer;
        }
    }

    void Rendering::SetVSyncEnabled(bool enabled)
    {
        _renderer->SetVSyncEnabled(enabled);
    }

    void Rendering::Submit(const RenderCommand& command)
    {
        if (_renderQueue.IsEmpty())
        {
            auto renderBatch = RenderBatch();
            renderBatch.AddCommand(command);
            _renderQueue.Enqueue(renderBatch);
        }
        else
        {
            auto& frameBatch = _renderQueue.Peek();
            frameBatch.AddCommand(command);
        }
    }

    void Rendering::Draw(const std::weak_ptr<IWindow>& surface)
    {
        if (_lastSurface.lock() != surface.lock())
        {
            // Update context
            _renderer->SetContext(surface);
            _renderer->SetVSyncEnabled(true); // Default to vsync on
            _lastSurface = surface;
        }

        // Update viewport
        const auto& surfaceSize = surface.lock()->GetSize();
        _renderer->SetViewport({ 0, 0 }, surfaceSize);

        // Process batch 
        // TODO: we prolly don't want to constantly draw and flush if nothing has changed....
        _renderer->BeginDraw();
        while (!_renderQueue.IsEmpty())
        {
            const auto& batch = _renderQueue.Dequeue();

            for (const auto& command : batch)
            {
                const auto& commandTypeId = typeid(command);

                if (commandTypeId == typeid(Tbx::ClearCommand))
                {
                    _renderer->Clear();
                }
                else if (commandTypeId == typeid(Tbx::DrawColorCommand))
                {
                    const auto& drawCommand = static_cast<const Tbx::DrawColorCommand&>(command);
                    _renderer->Draw(drawCommand.GetColor());
                }
                else if (commandTypeId == typeid(Tbx::DrawMeshCommand))
                {
                    const auto& drawCommand = static_cast<const Tbx::DrawMeshCommand&>(command);
                    _renderer->Draw(drawCommand.GetMesh());
                }
                else if (commandTypeId == typeid(Tbx::DrawTextureCommand))
                {
                    const auto& drawCommand = static_cast<const Tbx::DrawTextureCommand&>(command);
                    _renderer->Draw(drawCommand.GetTexture());
                }
                else if (commandTypeId == typeid(Tbx::DrawTextCommand))
                {
                    const auto& drawCommand = static_cast<const Tbx::DrawTextCommand&>(command);
                    _renderer->Draw(drawCommand.GetText());
                }
            }
        }
        _renderer->EndDraw();
        _renderer->Flush();
    }

    void Rendering::Flush()
    {
        _renderer->Flush();
        _renderQueue.Clear();
    }
}
