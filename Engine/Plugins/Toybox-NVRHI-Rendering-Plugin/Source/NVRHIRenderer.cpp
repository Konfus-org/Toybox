#include "NVRHIRenderer.h"
#ifdef TBX_PLATFORM_WINDOWS
#include <nvrhi/d3d12.h>
#endif
#include <nvrhi/vulkan.h>
#include <Tbx/Application/App/App.h>
#include <Tbx/Application/App/ApplicationEvents.h>
#include <Tbx/Systems/Windowing/WindowManager.h>
#include <Tbx/Systems/Events/EventCoordinator.h>
#include <Tbx/Systems/Debug/Debugging.h>
#include <Tbx/Systems/Rendering/IRenderer.h>
#include <Tbx/Systems/Plugins/RegisterPlugin.h>
#include <Tbx/Graphics/GraphicsSettings.h>
#include <Tbx/Graphics/Material.h>
#include <Tbx/Graphics/Shader.h>
#include <Tbx/Graphics/Texture.h>
#include <Tbx/Graphics/Mesh.h>

namespace NVRHIRendering
{
    NVRHIRenderer::~NVRHIRenderer()
    {
        Tbx::EventCoordinator::Unsubscribe<Tbx::WindowResizedEvent>(_windowResizedEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::WindowResizedEvent>(_graphicsSettingsChangedEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::RenderFrameRequest>(_renderFrameEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::ClearScreenRequest>(_clearScreenEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::FlushRendererRequest>(_flushEventId);

        Flush();
    }

    void NVRHIRenderer::Initialize(const std::shared_ptr<Tbx::IRenderSurface>& surface)
    {
        _windowResizedEventId =
            Tbx::EventCoordinator::Subscribe<Tbx::WindowResizedEvent>(TBX_BIND_FN(OnWindowResized));
        _graphicsSettingsChangedEventId =
            Tbx::EventCoordinator::Subscribe<Tbx::AppGraphicsSettingsChangedEvent>(TBX_BIND_FN(OnGraphicsSettingsChanged));
        _renderFrameEventId =
            Tbx::EventCoordinator::Subscribe<Tbx::RenderFrameRequest>(TBX_BIND_FN(OnRenderFrameRequest));
        _clearScreenEventId =
            Tbx::EventCoordinator::Subscribe<Tbx::ClearScreenRequest>(TBX_BIND_FN(OnClearScreenRequest));
        _flushEventId =
            Tbx::EventCoordinator::Subscribe<Tbx::FlushRendererRequest>(TBX_BIND_FN(OnFlushRequest));

        _nvrhiMessageHandler = std::make_unique<NVRHIMessageHandler>();

        _renderSurface = surface;
        SetViewport({ {0, 0}, surface->GetSize() });

        SetApi(Tbx::App::GetInstance()->GetGraphicsSettings().Api);
    }

    std::shared_ptr<Tbx::IRenderSurface> NVRHIRenderer::GetRenderSurface()
    {
        return _renderSurface;
    }

    Tbx::GraphicsDevice NVRHIRenderer::GetGraphicsDevice()
    {
        return _graphicsDevice.Get();
    }

    Tbx::GraphicsDeviceInfo NVRHIRenderer::GetGraphicsDeviceInfo()
    {
        return _graphicsDeviceInfo;
    }

    void NVRHIRenderer::SetApi(Tbx::GraphicsApi api)
    {
        _api = api;

        switch (api)
        {
            case Tbx::GraphicsApi::Vulkan:
            {
                // Create Vk instance
                VkInstance instance;
                {
                    auto appInfo = VkApplicationInfo();
                    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                    appInfo.pApplicationName = "Tbx App";
                    appInfo.pEngineName = "Toybox";
                    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
                    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
                    appInfo.apiVersion = VK_API_VERSION_1_2;

                    auto instanceCreateInfo = VkInstanceCreateInfo();
                    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
                    instanceCreateInfo.pApplicationInfo = &appInfo;

                    TBX_ASSERT(vkCreateInstance(&instanceCreateInfo, nullptr, &instance) == VK_SUCCESS, "Failed to create Vulkan instance");
                }

                // Select physical device (GPU)
                VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
                uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
                {
                    uint32_t deviceCount = 0;
                    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
                    std::vector<VkPhysicalDevice> devices(deviceCount);
                    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

                    for (const auto& device : devices)
                    {
                        uint32_t queueFamilyCount = 0;
                        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
                        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
                        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

                        for (uint32_t i = 0; i < queueFamilyCount; ++i)
                        {
                            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                            {
                                physicalDevice = device;
                                graphicsQueueFamilyIndex = i;
                                break;
                            }
                        }

                        if (physicalDevice != VK_NULL_HANDLE)
                            break;
                    }

                    TBX_ASSERT(physicalDevice != VK_NULL_HANDLE, "Failed to find suitable GPU!");
                }

                // Make logical device and queue
                VkDevice device;
                VkQueue graphicsQueue;
                {
                    float queuePriority = 1.0f;
                    auto queueCreateInfo = VkDeviceQueueCreateInfo();
                    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                    queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
                    queueCreateInfo.queueCount = 1;
                    queueCreateInfo.pQueuePriorities = &queuePriority;

                    auto deviceCreateInfo = VkDeviceCreateInfo();
                    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                    deviceCreateInfo.queueCreateInfoCount = 1;
                    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

                    TBX_ASSERT(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) == VK_SUCCESS, "Failed to create logical device!");

                    vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);
                }

                // Now we create the NVRHI graphics device
                {
                    auto deviceDesc = nvrhi::vulkan::DeviceDesc();
                    deviceDesc.errorCB = _nvrhiMessageHandler.get();
                    deviceDesc.instance = instance;
                    deviceDesc.physicalDevice = physicalDevice;
                    deviceDesc.device = device;
                    deviceDesc.graphicsQueue = graphicsQueue;
                    deviceDesc.graphicsQueueIndex = graphicsQueueFamilyIndex;
                    nvrhi::DeviceHandle nvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);

                    TBX_ASSERT(nvrhiDevice, "Failed to create NVRHI Vulkan device");

                    //_graphicsDeviceInfo = deviceDesc;
                    _graphicsDevice = nvrhiDevice;
                }

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

            case Tbx::GraphicsApi::DirectX12:
            {
#ifndef TBX_PLATFORM_WINDOWS
                TBX_ASSERT(false, "Cannot use DirectX on a non-Windows platform!");
#endif
                // Create DirectX 12 device
                auto deviceDesc = nvrhi::d3d12::DeviceDesc();
                deviceDesc.errorCB = _nvrhiMessageHandler.get();
                // Fill deviceDesc with your ID3D12Device, queues, descriptor sizes, etc.

                _graphicsDevice = nvrhi::d3d12::createDevice(deviceDesc);
                TBX_ASSERT(_graphicsDevice, "Failed to create DirectX NVRHI device!");
                break;
            }
            default:
            {
                TBX_ASSERT(false, "Unknown graphics API!");
                break;
            }
        }
    }

    Tbx::GraphicsApi NVRHIRenderer::GetApi()
    {
        return _api;
    }

    void NVRHIRenderer::SetViewport(const Tbx::Viewport& viewport)
    {
        _viewport = viewport;
    }

    const Tbx::Viewport& NVRHIRenderer::GetViewport()
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

    void NVRHIRenderer::Flush()
    {
    }

    void NVRHIRenderer::Clear(const Tbx::Color& color)
    {
    }

    void NVRHIRenderer::Draw(const Tbx::FrameBuffer& buffer)
    {
        for (const auto& item : buffer)
        {
            const auto& command = item.GetType();
            const auto& payload = item.GetPayload();
            switch (command)
            {
                case Tbx::DrawCommandType::None:
                {
                    TBX_VERBOSE("OpenGl Renderer: Processing none cmd...");
                    break;
                }
                case Tbx::DrawCommandType::Clear:
                {
                    TBX_VERBOSE("OpenGl Renderer: Processing clear cmd...");

                    const auto& color = std::any_cast<const Tbx::Color&>(payload);
                    Clear(color);
                    break;
                }
                case Tbx::DrawCommandType::CompileMaterial:
                {
                    TBX_VERBOSE("OpenGl Renderer: Processing material cmd...");

                    const auto& material = std::any_cast<const Tbx::Material&>(payload);

                    // TODO: Compile material using NVRHI API

                    break;
                }
                case Tbx::DrawCommandType::SetMaterial:
                {
                    TBX_VERBOSE("OpenGl Renderer: Processing material cmd...");

                    const auto& material = std::any_cast<const Tbx::Material&>(payload);

                    // TODO: Set material using NVRHI API

                    break;
                }
                case Tbx::DrawCommandType::DrawMesh:
                {
                    TBX_VERBOSE("OpenGl Renderer: Processing model cmd...");

                    const auto& mesh = std::any_cast<const Tbx::Mesh&>(payload);

                    //TODO: Draw mesh using NVRHI API

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

    void NVRHIRenderer::OnWindowFocusChanged(const Tbx::WindowFocusChangedEvent& e)
    {
        std::shared_ptr<Tbx::IWindow> windowThatWasFocused = Tbx::WindowManager::GetWindow(e.GetWindowId());
        Initialize(windowThatWasFocused);
    }

    void NVRHIRenderer::OnWindowResized(const Tbx::WindowResizedEvent& e)
    {
        std::shared_ptr<Tbx::IWindow> windowThatWasResized = Tbx::WindowManager::GetWindow(e.GetWindowId());
        if (windowThatWasResized != GetRenderSurface()) return;

        bool oldVsyncEnabled = GetVSyncEnabled();

        // Enable vsync so the window doesn't flicker
        SetVSyncEnabled(true);

        // Draw the window while its resizing so there are no artifacts during the resize
        SetViewport(Tbx::Viewport{ {0, 0}, e.GetNewSize() });
        Clear(_clearColor);

        // Set vsync back to what it was
        SetVSyncEnabled(oldVsyncEnabled);
    }

    void NVRHIRenderer::OnGraphicsSettingsChanged(const Tbx::AppGraphicsSettingsChangedEvent& e)
    {
        const auto& settings = e.GetNewSettings();

        _clearColor = settings.ClearColor;
        SetVSyncEnabled(settings.VSyncEnabled);
        SetResolution(settings.Resolution);
    }

    void NVRHIRenderer::OnRenderFrameRequest(Tbx::RenderFrameRequest& e)
    {
        Flush();
        Clear(_clearColor);
        Draw(e.GetBuffer());

        auto renderedFrameEvent = Tbx::RenderedFrameEvent();
        Tbx::EventCoordinator::Send<Tbx::RenderedFrameEvent>(renderedFrameEvent);

        e.IsHandled = true;
    }

    void NVRHIRenderer::OnClearScreenRequest(Tbx::ClearScreenRequest& e)
    {
        Clear(_clearColor);
        e.IsHandled = true;
    }

    void NVRHIRenderer::OnFlushRequest(Tbx::FlushRendererRequest& e)
    {
        Flush();
        e.IsHandled = true;
    }
}
