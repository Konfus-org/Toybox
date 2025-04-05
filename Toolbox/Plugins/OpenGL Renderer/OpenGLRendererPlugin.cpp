#include "OpenGLRendererPlugin.h"
#include <Tbx/Core/Events/EventDispatcher.h>

namespace OpenGLRendering
{
    void OpenGLRendererPlugin::OnLoad()
    {
        _setRenderContextEventId = Tbx::EventDispatcher::Subscribe<Tbx::SetRenderContextRequestEvent>(TBX_BIND_CALLBACK(OnSetRenderContextEvent));
        _setVSyncEventId = Tbx::EventDispatcher::Subscribe<Tbx::SetVSyncRequestEvent>(TBX_BIND_CALLBACK(OnSetVSyncEvent));

        // TODO: something here is wrong.. we are leaking memory like crazy on begin frame...
        ////_beginRenderFrameEventId = Tbx::EventDispatcher::Subscribe<Tbx::BeginRenderFrameRequestEvent>(TBX_BIND_CALLBACK(OnBeginRenderFrameEvent));
        ////_renderFrameEventId = Tbx::EventDispatcher::Subscribe<Tbx::RenderFrameRequestEvent>(TBX_BIND_CALLBACK(OnRenderFrameEvent));
        ////_endRenderFrameEventId = Tbx::EventDispatcher::Subscribe<Tbx::EndRenderFrameRequestEvent>(TBX_BIND_CALLBACK(OnEndRenderFrameEvent));

        //_clearScreenEventId = Tbx::EventDispatcher::Subscribe<Tbx::ClearScreenRequestEvent>(TBX_BIND_CALLBACK(OnClearScreenRenderEvent));
        _flushEventId = Tbx::EventDispatcher::Subscribe<Tbx::FlushRendererRequestEvent>(TBX_BIND_CALLBACK(OnFlushEvent));
    }

    void OpenGLRendererPlugin::OnUnload()
    {
        Tbx::EventDispatcher::Unsubscribe<Tbx::SetRenderContextRequestEvent>(_setRenderContextEventId);
        Tbx::EventDispatcher::Unsubscribe<Tbx::SetVSyncRequestEvent>(_setVSyncEventId);

        //Tbx::EventDispatcher::Unsubscribe<Tbx::BeginRenderFrameRequestEvent>(_beginRenderFrameEventId);
        //Tbx::EventDispatcher::Unsubscribe<Tbx::RenderFrameRequestEvent>(_renderFrameEventId);
        //Tbx::EventDispatcher::Unsubscribe<Tbx::EndRenderFrameRequestEvent>(_endRenderFrameEventId);

        //Tbx::EventDispatcher::Unsubscribe<Tbx::ClearScreenRequestEvent>(_clearScreenEventId);
        Tbx::EventDispatcher::Unsubscribe<Tbx::FlushRendererRequestEvent>(_flushEventId);
    }

    void OpenGLRendererPlugin::OnSetRenderContextEvent(Tbx::SetRenderContextRequestEvent& e)
    {
        auto viewportSize = e.GetContext().lock()->GetSize();
        SetContext(e.GetContext());
        e.IsHandled = true;
    }

    void OpenGLRendererPlugin::OnSetVSyncEvent(Tbx::SetVSyncRequestEvent& e)
    {
        SetVSyncEnabled(e.GetVSync());
        e.IsHandled = true;
    }

    void OpenGLRendererPlugin::OnBeginRenderFrameEvent(Tbx::BeginRenderFrameRequestEvent& e)
    {
        Flush();
        Clear();
        BeginDraw();
        e.IsHandled = true;
    }

    void OpenGLRendererPlugin::OnRenderFrameEvent(Tbx::RenderFrameRequestEvent& e)
    {
        const auto& batch = e.GetBatch();
        //for (const auto& item : batch)
        //{
        //    switch (item.Command)
        //    {
        //        case Tbx::RenderCommand::Clear:
        //        {
        //            const auto& colorData = std::any_cast<Tbx::Color>(item.Data);
        //            Clear(colorData);
        //            break;
        //        }
        //        case Tbx::RenderCommand::UploadShader:
        //        {
        //            const auto& shaderData = std::any_cast<Tbx::Shader>(item.Data);
        //            UploadShader(shaderData);
        //            break;
        //        }
        //        case Tbx::RenderCommand::UploadTexture:
        //        {
        //            const auto& textureData = std::any_cast<Tbx::TextureRenderData>(item.Data);
        //            UploadTexture(textureData.GetTexture(), textureData.GetSlot());
        //            break;
        //        }
        //        case Tbx::RenderCommand::UploadShaderData:
        //        {
        //            const auto& shaderData = std::any_cast<Tbx::ShaderData>(item.Data);
        //            UploadShaderData(shaderData);
        //            break;
        //        }
        //        case Tbx::RenderCommand::RenderMesh:
        //        {
        //            const auto& meshData = std::any_cast<Tbx::MeshRenderData>(item.Data);
        //            Draw(meshData.GetMesh(), meshData.GetMaterial());
        //            break;
        //        }
        //        default:
        //        {
        //            TBX_ASSERT(false, "Unknown render command type.");
        //            break;
        //        }
        //    }
        //}
    }

    void OpenGLRendererPlugin::OnEndRenderFrameEvent(Tbx::EndRenderFrameRequestEvent& e)
    {
        EndDraw();
        e.IsHandled = true;
    }

    void OpenGLRendererPlugin::OnClearScreenRenderEvent(Tbx::ClearScreenRequestEvent& e)
    {
        Clear(Tbx::Color::DarkGrey());
        e.IsHandled = true;
    }

    void OpenGLRendererPlugin::OnFlushEvent(Tbx::FlushRendererRequestEvent& e)
    {
        Flush();
        e.IsHandled = true;
    }
}

TBX_REGISTER_PLUGIN(OpenGLRendering::OpenGLRendererPlugin);