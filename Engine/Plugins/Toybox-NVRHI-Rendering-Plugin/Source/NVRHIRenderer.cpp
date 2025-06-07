#include "NVRHIRenderer.h"
#ifdef TBX_PLATFORM_WINDOWS
#include <nvrhi/d3d12.h>
#endif
#include <nvrhi/vulkan.h>

namespace NVRHIRendering
{
    NVRHIRenderer::NVRHIRenderer()
    {
    }

    void NVRHIRenderer::SetApi(Tbx::GraphicsApi api)
    {
        switch (api)
        {
            case Tbx::GraphicsApi::Vulkan:
            {
                //// Create Vulkan device
                //nvrhi::vulkan::DeviceDesc deviceDesc = {};
                //// Fill deviceDesc with your VkDevice, VkPhysicalDevice, VkInstance, queues, etc.
                //// These should be passed in or wrapped by your engine already

                //_device = nvrhi::vulkan::createDevice(deviceDesc);
                TBX_ASSERT(_device, "Failed to create Vulkan NVRHI device!");
                break;
            }

            case Tbx::GraphicsApi::Metal:
            {
#ifndef TBX_PLATFORM_OSX
                TBX_ASSERT(false, "Cannot use Metal on a non-Apple platform!");
#else
                TBX_ASSERT(false, "Metal is not supported by NVRHI.");
#endif
                break;
            }

            case Tbx::GraphicsApi::DirectX:
            {
#ifndef TBX_PLATFORM_WINDOWS
                TBX_ASSERT(false, "Cannot use DirectX on a non-Windows platform!");
#endif
                // Create DirectX 12 device
                nvrhi::d3d12::DeviceDesc deviceDesc = {};
                // Fill deviceDesc with your ID3D12Device, queues, descriptor sizes, etc.

                _device = nvrhi::d3d12::createDevice(deviceDesc);
                TBX_ASSERT(_device, "Failed to create DirectX NVRHI device!");
                break;
            }
            default:
            {
                TBX_ASSERT(false, "Unknown graphics API!");
                break;
            }
        }
    }

    void NVRHIRenderer::SetRenderSurface(const std::shared_ptr<Tbx::IRenderSurface>& surface)
    {
        _renderSurface = surface;
    }

    void NVRHIRenderer::SetViewport(const Tbx::ViewPort& viewport)
    {
        _viewport = viewport;
    }

    const Tbx::ViewPort& NVRHIRenderer::GetViewport()
    {
        return _viewport;
    }

    void NVRHIRenderer::SetResolution(const Tbx::Size& size)
    {
        _resolution = size;
    }

    const Tbx::Size& NVRHIRenderer::GetResolution()
    {
        return _resolution;
    }

    void NVRHIRenderer::SetVSyncEnabled(bool enabled)
    {
        _vsyncEnabled = enabled;
    }

    bool NVRHIRenderer::GetVSyncEnabled()
    {
        return _vsyncEnabled;
    }

    std::any NVRHIRenderer::GetGraphicsDevice()
    {
        return std::any();
    }

    void NVRHIRenderer::RenderFrame(const Tbx::FrameBuffer& buffer)
    {
        for (const auto& item : buffer)
        {
            const auto& command = item.GetCommand();
            const auto& payload = item.GetPayload();
            switch (command)
            {
                case Tbx::RenderCommand::None:
                {
                    TBX_VERBOSE("OpenGl Renderer: Processing none cmd...");
                    break;
                }
                case Tbx::RenderCommand::Clear:
                {
                    TBX_VERBOSE("OpenGl Renderer: Processing clear cmd...");

                    const auto& color = std::any_cast<const Tbx::Color&>(payload);
                    Clear(color);
                    break;
                }
                case Tbx::RenderCommand::RenderModel:
                {
                    TBX_VERBOSE("OpenGl Renderer: Processing model cmd...");

                    const auto& mesh = std::any_cast<const Tbx::Model&>(payload);
                    Draw(mesh);
                    break;
                }
                default:
                {
                    TBX_ASSERT(false, "Unknown render command type.");
                    break;
                }
            }
        }
    }

    void NVRHIRenderer::Draw(const Tbx::Model& model)
    {
        // TODO: nab models material (a shader, color, and set of textures) and mesh (vertices and indices) then use NVRHI to send to GPU
    }

    void NVRHIRenderer::BeginDraw()
    {
    }

    void NVRHIRenderer::EndDraw()
    {
    }
}
