#include "SDLRenderer.h"
#include <Tbx/Debug/Debugging.h>
#include <Tbx/Graphics/Material.h>
#include <Tbx/Graphics/Mesh.h>
#include <Tbx/App/App.h>

namespace SDLRendering
{
    SDLRenderer::~SDLRenderer()
    {
        Shutdown();
    }

    void SDLRenderer::Initialize(const std::shared_ptr<Tbx::IRenderSurface>& surface)
    {
        auto* window = (SDL_Window*)surface->GetNativeWindow();
        _surface = surface;
        TBX_ASSERT(_surface, "No surface to render to was given!");

#ifdef TBX_DEBUG
        bool debug_mode = true;
#else
        bool debug_mode = false;
#endif
        // create a device for either VULKAN, METAL, or DX12 with debugging enabled and choose the best driver
        _device = std::shared_ptr<SDL_GPUDevice>(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_DXBC, debug_mode, NULL), [](SDL_GPUDevice* deviceToDelete)
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

    void SDLRenderer::Shutdown()
    {
        _cachedTextureManager.Release(_device.get());
        _cachedShaderManager.Release(_device.get());
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
        SDL_Window* window = (SDL_Window*)_surface.get()->GetNativeWindow();

        // acquire the command buffer
        _currCommandBuffer = SDL_AcquireGPUCommandBuffer(_device.get());

        // get the swapchain texture
        Uint32 width, height;
        SDL_WaitAndAcquireGPUSwapchainTexture(_currCommandBuffer, window, &_currSwapchainTexture, &width, &height);

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
                    _material = material;
                    _shaderDatas.clear();
                    break;
                }
                case Tbx::DrawCommandType::UploadMaterialData:
                {
                    const auto& data = std::any_cast<const Tbx::ShaderData&>(cmd.GetPayload());
                    _shaderDatas.push_back(data);
                    break;
                }
                case Tbx::DrawCommandType::DrawMesh:
                {
                    SDL_EndGPURenderPass(_currRenderPass);

                    const auto& mesh = std::any_cast<const Tbx::Mesh&>(cmd.GetPayload());
                    const Tbx::VertexBuffer& meshVertexBuffer = mesh.GetVertexBuffer();
                    const Tbx::BufferLayout& meshBufferLayout = meshVertexBuffer.GetLayout();

                    // create the textures (if not already)
                    const std::vector<Tbx::Texture>& textures = _material.GetTextures();
                    for (size_t i=0; i<textures.size(); i++)
                    {
                        const Tbx::Texture& texture = textures[i];
                        _cachedTextureManager.Add(_device.get(), _currCommandBuffer, texture);
                    }

                    // create the shaders (if not already)
                    const Tbx::Shader shader = _material.GetShader();
                    SDLCachedShader cachedVertexShader = _cachedShaderManager.AddVertexShader(_device.get(), shader);
                    SDLCachedShader cachedFragmentShader = _cachedShaderManager.AddFragmentShader(_device.get(), shader);

                    // get the vertices from the mesh
                    const std::vector<float>& vertices = meshVertexBuffer.GetVertices();
                    Tbx::uint32 verticesSize = static_cast<Uint32>(sizeof(float) * vertices.size());
                    const void* pVertices = &vertices[0];

                    // get the indices from the mesh
                    const std::vector<Tbx::uint32>& indices = mesh.GetIndices();
                    Tbx::uint32 indicesSize = static_cast<Uint32>(sizeof(Tbx::uint32) * indices.size());
                    Uint32 indexCount = static_cast<Uint32>(indices.size());
                    const void* pIndices = &indices[0];

                    std::vector<SDL_GPUVertexAttribute> vertexAttributes;
                    SDLCreateVertexAttributes(vertexAttributes, meshBufferLayout);

                    std::vector<SDL_GPUVertexBufferDescription> vertexBufferDesctiptions;
                    SDLCreateVertexBufferDescriptions(vertexBufferDesctiptions, meshBufferLayout);

                    SDL_GPUColorTargetDescription colorTargetDescriptions[1];
                    colorTargetDescriptions[0] = {};
                    colorTargetDescriptions[0].format = SDL_GetGPUSwapchainTextureFormat(_device.get(), window);

                    // create the graphics pipeline
                    SDL_GPUGraphicsPipelineCreateInfo graphicsPipelineInfo = {};
                    graphicsPipelineInfo.vertex_shader = cachedVertexShader._shader;
                    graphicsPipelineInfo.fragment_shader = cachedFragmentShader._shader;
                    graphicsPipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
                    graphicsPipelineInfo.vertex_input_state.num_vertex_attributes = (Uint32)vertexAttributes.size();
                    graphicsPipelineInfo.vertex_input_state.vertex_attributes = &vertexAttributes[0];
                    graphicsPipelineInfo.vertex_input_state.num_vertex_buffers = (Uint32)vertexBufferDesctiptions.size();
                    graphicsPipelineInfo.vertex_input_state.vertex_buffer_descriptions = &vertexBufferDesctiptions[0];
                    graphicsPipelineInfo.target_info.num_color_targets = 1;
                    graphicsPipelineInfo.target_info.color_target_descriptions = colorTargetDescriptions;
                    //graphicsPipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
                    SDL_GPUGraphicsPipeline* graphicsPipeline = SDL_CreateGPUGraphicsPipeline(_device.get(), &graphicsPipelineInfo);

                    // create the vertex buffer and upload the vertices
                    SDL_GPUBufferCreateInfo vertexBufferCreateInfo = {};
                    vertexBufferCreateInfo.size = verticesSize;
                    vertexBufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
                    SDL_GPUBuffer* vertexBuffer = SDLCreateBuffer(_device.get(), vertexBufferCreateInfo);
                    SDLUploadBuffer(_device.get(), _currCommandBuffer, vertexBuffer, verticesSize, pVertices);

                    // create the index buffer and upload the indices
                    SDL_GPUBufferCreateInfo indexBufferCreateInfo = {};
                    indexBufferCreateInfo.size = indicesSize;
                    indexBufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
                    SDL_GPUBuffer* indexBuffer = SDLCreateBuffer(_device.get(), indexBufferCreateInfo);
                    SDLUploadBuffer(_device.get(), _currCommandBuffer, indexBuffer, indicesSize, pIndices);

                    colorTargetInfo.load_op = SDL_GPU_LOADOP_LOAD; // don't clear color target
                    _currRenderPass = SDL_BeginGPURenderPass(_currCommandBuffer, &colorTargetInfo, 1, NULL);
                    SDL_BindGPUGraphicsPipeline(_currRenderPass, graphicsPipeline);

                    // bind the vertex buffer
                    SDL_GPUBufferBinding vertexBufferBindings[1];
                    vertexBufferBindings[0] = {};
                    vertexBufferBindings[0].buffer = vertexBuffer;
                    vertexBufferBindings[0].offset = 0;
                    SDL_BindGPUVertexBuffers(_currRenderPass, 0, vertexBufferBindings, 1);

                    // bind the index buffer
                    SDL_GPUBufferBinding indexBufferBindings[1];
                    indexBufferBindings[0] = {};
                    indexBufferBindings[0].buffer = indexBuffer;
                    indexBufferBindings[0].offset = 0;
                    SDL_BindGPUIndexBuffer(_currRenderPass, indexBufferBindings, SDL_GPU_INDEXELEMENTSIZE_32BIT);

                    // upload the uniform data to the shaders
#if USE_SHADER_UNIFORM_HACKERY
                    for (size_t i=0; i<_shaderDatas.size(); i++)
                    {
                        const Tbx::ShaderData& shaderData = _shaderDatas[i];
                        if (shaderData._isFragment)
                        {
                            SDL_PushGPUFragmentUniformData(_currCommandBuffer, shaderData._uniformSlot, shaderData._uniformData, shaderData._uniformSize);
                        }
                        else
                        {
                            SDL_PushGPUVertexUniformData(_currCommandBuffer, shaderData._uniformSlot, shaderData._uniformData, shaderData._uniformSize);
                        }
                    }
#endif

                    // bind the textures to the fragment shader
                    for (size_t i=0; i<textures.size(); i++)
                    {
                        const Tbx::Texture& texture = textures[i];
                        SDLCachedTexture cachedTexture = _cachedTextureManager.Get(texture);
                        if (cachedTexture._sampler != nullptr)
                        {
                            SDL_GPUTextureSamplerBinding textureSamplerBinding = {};
                            textureSamplerBinding.texture = cachedTexture._texture;
                            textureSamplerBinding.sampler = cachedTexture._sampler;
                            SDL_BindGPUFragmentSamplers(_currRenderPass, 0, &textureSamplerBinding, 1);
                        }
                    }

                    // draw the mesh
                    SDL_DrawGPUIndexedPrimitives(_currRenderPass, indexCount, 1, 0, 0, 0);

                    // release the resources created above
                    SDL_ReleaseGPUGraphicsPipeline(_device.get(), graphicsPipeline);
                    SDL_ReleaseGPUBuffer(_device.get(), indexBuffer);
                    SDL_ReleaseGPUBuffer(_device.get(), vertexBuffer);

                    /*
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
                    */
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
