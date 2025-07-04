#include "SDLRenderer.h"
#include <Tbx/Debug/Debugging.h>
#include <Tbx/Graphics/Material.h>
#include <Tbx/Graphics/Mesh.h>
#include <Tbx/App/App.h>

namespace SDLRendering
{
    void SDLRenderer::Initialize(const std::shared_ptr<Tbx::IRenderSurface>& surface)
    {
        auto* window = (SDL_Window*)surface->GetNativeWindow();
        _surface = surface;
        TBX_ASSERT(_surface, "No surface to render to was given!");

        // create a device for either VULKAN, METAL, or DX12 with debugging enabled and choose the best driver
        _device = std::shared_ptr<SDL_GPUDevice>(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_DXBC, false, NULL), [](SDL_GPUDevice* deviceToDelete)
        { 
            SDL_DestroyGPUDevice(deviceToDelete);
        });
        TBX_ASSERT(_device, "Failed to create SDL_Renderer: {}", SDL_GetError());

        SDL_ClaimWindowForGPUDevice(_device.get(), window);

        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        _resolution = { w, h };
        _viewport = { { 0, 0 }, { w, h } };

        std::string err = SDL_GetError();
        TBX_ASSERT(err.empty(), "An error from SDL has occured: {}", err);
    }

    Tbx::GraphicsDevice SDLRenderer::GetGraphicsDevice()
    {
        return _device.get();
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

        std::string err = SDL_GetError();
        TBX_ASSERT(err.empty(), "An error from SDL has occured: {}", err);
    }

    const Tbx::Viewport& SDLRenderer::GetViewport()
    {
        return _viewport;
    }

    void SDLRenderer::SetResolution(const Tbx::Size& size)
    {
        _resolution = size;
    }

    const Tbx::Size& SDLRenderer::GetResolution()
    {
        return _resolution;
    }

    void SDLRenderer::SetVSyncEnabled(bool enabled)
    {
        _vsyncEnabled = enabled;
    }

    bool SDLRenderer::GetVSyncEnabled()
    {
        return _vsyncEnabled;
    }

    void SDLRenderer::Flush()
    {
    }

    void SDLRenderer::Clear(const Tbx::Color& color)
    {
        if (_currRenderPass != nullptr)
        {
            // End the current render pass if we have already started one
            SDL_EndGPURenderPass(_currRenderPass);
            _currRenderPass = nullptr;
        }

        // Start new render pass with new clear color
        SDL_GPUColorTargetInfo colorTargetInfo {};
        colorTargetInfo.clear_color = { color.R * 255, color.G * 255, color.B * 255, color.A * 255 };
        colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
        colorTargetInfo.texture = _currSwapchainTexture;
        _currRenderPass = SDL_BeginGPURenderPass(_currCommandBuffer, &colorTargetInfo, 1, NULL);

        std::string err = SDL_GetError();
        TBX_ASSERT(err.empty(), "An error from SDL has occured: {}", err);
    }

    void SDLRenderer::Draw(const Tbx::FrameBuffer& buffer)
    {
        // acquire the command buffer
        _currCommandBuffer = SDL_AcquireGPUCommandBuffer(_device.get());

        // get the swapchain texture
        Uint32 width, height;
        SDL_WaitAndAcquireGPUSwapchainTexture(_currCommandBuffer, (SDL_Window*)_surface->GetNativeWindow(), &_currSwapchainTexture, &width, &height);

        // end the frame early if a swapchain texture is not available
        if (_currSwapchainTexture == nullptr)
        {
            // you must always submit the command buffer
            SDL_SubmitGPUCommandBuffer(_currCommandBuffer);
            return;
        }

        // create the color target
        auto colorTargetInfo = SDL_GPUColorTargetInfo();
        auto clearColor = Tbx::App::GetInstance()->GetGraphicsSettings().ClearColor;
        colorTargetInfo.clear_color = { clearColor.R, clearColor.G, clearColor.B, clearColor.A };
        colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
        colorTargetInfo.texture = _currSwapchainTexture;

        // begin a render pass
        _currRenderPass = SDL_BeginGPURenderPass(_currCommandBuffer, &colorTargetInfo, 1, NULL);

        // draw something
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
                    auto material = std::any_cast<const Tbx::Material&>(cmd.GetPayload());
                    break;
                }
                case Tbx::DrawCommandType::SetMaterial:
                {
                    auto material = std::any_cast<const Tbx::Material&>(cmd.GetPayload());
                    break;
                }
                case Tbx::DrawCommandType::UploadMaterialData:
                {
                    //const auto& data = std::any_cast<const Tbx::ShaderData&>(cmd.GetPayload());
                    TBX_ASSERT(false, "NOT YET IMPLEMENTED!");
                    break;
                }
                case Tbx::DrawCommandType::DrawMesh:
                {
                    const auto& mesh = std::any_cast<const Tbx::Mesh&>(cmd.GetPayload());
                    const auto& meshVerts = mesh.GetVertices();

                    // create the vertex buffer
                    auto bufferInfo = SDL_GPUBufferCreateInfo();
                    bufferInfo.size = sizeof(meshVerts);
                    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
                    SDL_GPUBuffer* vertexBuffer = SDL_CreateGPUBuffer(_device.get(), &bufferInfo);

                    // create a transfer buffer to upload to the vertex buffer
                    auto transferInfo = SDL_GPUTransferBufferCreateInfo();
                    transferInfo.size = sizeof(meshVerts);
                    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(_device.get(), &transferInfo);

                    // map the transfer buffer to a pointer
                    Tbx::Vertex* data = (Tbx::Vertex*)SDL_MapGPUTransferBuffer(_device.get(), transferBuffer, false);
                    SDL_memcpy(data, &meshVerts, sizeof(meshVerts));

                    // copy data to GPU
                    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(_currCommandBuffer);

                    // where is the data
                    auto location = SDL_GPUTransferBufferLocation();
                    location.transfer_buffer = transferBuffer;
                    location.offset = 0; // start from the beginning

                    // where to upload the data
                    auto region = SDL_GPUBufferRegion();
                    region.buffer = vertexBuffer;
                    region.size = sizeof(meshVerts); // size of the data in bytes
                    region.offset = 0; // begin writing from the first vertex

                    // upload the data
                    SDL_UploadToGPUBuffer(copyPass, &location, &region, true);

                    // release resources
                    SDL_EndGPUCopyPass(copyPass);
                    SDL_ReleaseGPUBuffer(_device.get(), vertexBuffer);
                    SDL_UnmapGPUTransferBuffer(_device.get(), transferBuffer);
                    SDL_ReleaseGPUTransferBuffer(_device.get(), transferBuffer);

                    break;
                }
                default:
                    break;
            }
        }

        // end the render pass
        SDL_EndGPURenderPass(_currRenderPass);
        _currRenderPass = nullptr;

        // submit the command buffer
        SDL_SubmitGPUCommandBuffer(_currCommandBuffer);
        _currCommandBuffer = nullptr;

        std::string err = SDL_GetError();
        TBX_ASSERT(err.empty(), "An error from SDL has occured: {}", err);
    }
}
