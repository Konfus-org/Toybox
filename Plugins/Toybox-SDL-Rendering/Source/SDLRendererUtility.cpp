#include "SDLRendererUtility.h"
#include "SDLRenderer.h"
#include <Tbx/Debug/Debugging.h>
#include <SDL3_shadercross/SDL_shadercross.h>

namespace SDLRendering
{
    SDLCachedTexture::SDLCachedTexture()
    {
        _texture = nullptr;
        _sampler = nullptr;
    }

    void SDLCachedTexture::Release(SDL_GPUDevice* device)
    {
        if (_texture != nullptr)
        {
            SDL_ReleaseGPUTexture(device, _texture);
            _texture = nullptr;
        }

        if (_sampler != nullptr)
        {
            SDL_ReleaseGPUSampler(device, _sampler);
            _sampler = nullptr;
        }
    }

    void SDLCachedTextureManager::Release(SDL_GPUDevice* device)
    {
        for (std::map<const std::string, SDLCachedTexture>::iterator i = _cachedTextures.begin(); i != _cachedTextures.end(); i++)
        {
            SDLCachedTexture& cachedTexture = i->second;
            cachedTexture.Release(device);
        }
        _cachedTextures.clear();
    }

    SDLCachedTexture SDLCachedTextureManager::Add(SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer, const Tbx::Texture& texture)
    {
        const std::string& texturePath = texture.GetPath();

        std::map<const std::string, SDLCachedTexture>::iterator i = _cachedTextures.find(texturePath);
        if (i == _cachedTextures.end())
        {
            Tbx::TextureFilter textureFilter = texture.GetFilter();
            Tbx::TextureWrap textureWrap = texture.GetWrap();

            SDLCachedTexture cachedTexture;

            // create the texture
            SDL_Surface* surface = SDLLoadSurface(texturePath.c_str(), SDL_PIXELFORMAT_ABGR8888);
            if (surface != nullptr)
            {
                Uint32 textureWidth = static_cast<Uint32>(surface->w);
                Uint32 textureHeight = static_cast<Uint32>(surface->h);
                Uint32 textureSize = textureWidth * textureHeight * 4;
                const void* textureData = surface->pixels;

                SDL_GPUTextureCreateInfo textureCreateInfo = {};
                textureCreateInfo.type = SDL_GPU_TEXTURETYPE_2D;
                textureCreateInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
                textureCreateInfo.width = textureWidth;
                textureCreateInfo.height = textureHeight;
                textureCreateInfo.layer_count_or_depth = 1;
                textureCreateInfo.num_levels = 1;
                textureCreateInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
                cachedTexture._texture = SDLCreateTexture(device, textureCreateInfo);

                SDLUploadTexture(device, commandBuffer, cachedTexture._texture, textureSize, textureData, textureWidth, textureHeight);
            }

            // create the sampler
            SDL_GPUSamplerCreateInfo samplerCreateInfo = {};
            switch(textureFilter)
            {
                case Tbx::TextureFilter::Nearest:
                    samplerCreateInfo.min_filter = SDL_GPU_FILTER_NEAREST;
                    samplerCreateInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
                    break;
                case Tbx::TextureFilter::Linear:
                    samplerCreateInfo.min_filter = SDL_GPU_FILTER_LINEAR;
                    samplerCreateInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
                    break;
            }
            samplerCreateInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            switch(textureWrap)
            {
                case Tbx::TextureWrap::ClampToEdge:
                    samplerCreateInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                    samplerCreateInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                    samplerCreateInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                    break;
                case Tbx::TextureWrap::MirroredRepeat:
                    samplerCreateInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
                    samplerCreateInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
                    samplerCreateInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
                    break;
                case Tbx::TextureWrap::Repeat:
                    samplerCreateInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
                    samplerCreateInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
                    samplerCreateInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
                    break;
            }
            cachedTexture._sampler = SDL_CreateGPUSampler(device, &samplerCreateInfo);

            _cachedTextures.insert(std::make_pair(texturePath, cachedTexture));
        }

        return _cachedTextures.find(texturePath)->second;
    }

    SDLCachedTexture SDLCachedTextureManager::Get(const Tbx::Texture& texture)
    {
        const std::string& texturePath = texture.GetPath();
        std::map<const std::string, SDLCachedTexture>::iterator i = _cachedTextures.find(texturePath);
        if (i != _cachedTextures.end())
        {
            return i->second;
        }
        else
        {
            return SDLCachedTexture();
        }
    }

    SDLCachedShader::SDLCachedShader()
    {
        _shader = nullptr;
    }

    void SDLCachedShader::Release(SDL_GPUDevice* device)
    {
        if (_shader != nullptr)
        {
            SDL_ReleaseGPUShader(device, _shader);
            _shader = nullptr;
        }
    }

    void SDLCachedShaderManager::Release(SDL_GPUDevice* device)
    {
        for (std::map<const std::string, SDLCachedShader>::iterator i = _cachedShaders.begin(); i != _cachedShaders.end(); i++)
        {
            SDLCachedShader& cachedShader = i->second;
            cachedShader.Release(device);
        }
        _cachedShaders.clear();
    }

    SDLCachedShader SDLCachedShaderManager::AddVertexShader(SDL_GPUDevice* device, const Tbx::Shader& shader)
    {
        const std::string& shaderPath = shader.GetVertexSource();

        std::map<const std::string, SDLCachedShader>::iterator i = _cachedShaders.find(shaderPath);
        if (i == _cachedShaders.end())
        {
            SDLCachedShader cachedShader;

            SDL_GPUShader* vertexShader = nullptr;

            size_t shaderCodeSize;
            void* shaderCode = SDL_LoadFile(shaderPath.c_str(), &shaderCodeSize);
            if (shaderCode != nullptr)
            {
                SDL_ShaderCross_SPIRV_Info vertexInfo = {};
                if(shaderPath.ends_with(".hlsl"))
                {
                    SDL_ShaderCross_HLSL_Info info = {};
                    info.source = (const char*)shaderCode;
                    info.entrypoint = "main";
                    info.include_dir = NULL;
                    info.defines = NULL;
                    info.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
                    info.enable_debug = true;
                    info.name = NULL;
                    size_t size = 0;
                    void* data = SDL_ShaderCross_CompileSPIRVFromHLSL(&info, &size);
                    TBX_ASSERT(data != nullptr && size != 0, "Failed to compile vertex shader");
                    vertexInfo.bytecode = (Uint8*)data;
                    vertexInfo.bytecode_size = size;
                }
                else
                {
                    vertexInfo.bytecode = (Uint8*)shaderCode;
                    vertexInfo.bytecode_size = shaderCodeSize;
                }
                vertexInfo.entrypoint = "main";
                vertexInfo.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
                vertexInfo.enable_debug = true;

                SDL_ShaderCross_GraphicsShaderMetadata vertexMetadata = {};

                vertexMetadata.num_uniform_buffers = 1;
                vertexMetadata.num_samplers = 0;
                vertexMetadata.num_storage_textures = 0;
                vertexMetadata.num_storage_buffers = 0;
                vertexMetadata.num_inputs = 0;
                vertexMetadata.num_outputs = 0;
                vertexMetadata.inputs = nullptr;
                vertexMetadata.outputs = nullptr;

                vertexShader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &vertexInfo, &vertexMetadata, 0);
                SDL_free(shaderCode);
            }

            cachedShader._shader = vertexShader;

            _cachedShaders.insert(std::make_pair(shaderPath, cachedShader));
        }

        return _cachedShaders.find(shaderPath)->second;
    }

    SDLCachedShader SDLCachedShaderManager::AddFragmentShader(SDL_GPUDevice* device, const Tbx::Shader& shader)
    {
        const std::string& shaderPath = shader.GetFragmentSource();

        std::map<const std::string, SDLCachedShader>::iterator i = _cachedShaders.find(shaderPath);
        if (i == _cachedShaders.end())
        {
            SDLCachedShader cachedShader;

            SDL_GPUShader* fragmentShader = nullptr;

            size_t shaderCodeSize;
            void* shaderCode = SDL_LoadFile(shaderPath.c_str(), &shaderCodeSize);
            if (shaderCode != nullptr)
            {
                SDL_ShaderCross_SPIRV_Info fragmentInfo = {};
                if(shaderPath.ends_with(".hlsl"))
                {
                    SDL_ShaderCross_HLSL_Info info = {};
                    info.source = (const char*)shaderCode;
                    info.entrypoint = "main";
                    info.include_dir = NULL;
                    info.defines = NULL;
                    info.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
                    info.enable_debug = true;
                    info.name = NULL;
                    size_t size = 0;
                    void* data = SDL_ShaderCross_CompileSPIRVFromHLSL(&info, &size);
                    TBX_ASSERT(data != nullptr && size != 0, "Failed to compile fragment shader");
                    fragmentInfo.bytecode = (Uint8*)data;
                    fragmentInfo.bytecode_size = size;
                }
                else
                {
                    fragmentInfo.bytecode = (Uint8*)shaderCode;
                    fragmentInfo.bytecode_size = shaderCodeSize;
                }
                fragmentInfo.entrypoint = "main";
                fragmentInfo.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
                fragmentInfo.enable_debug = true;

                SDL_ShaderCross_GraphicsShaderMetadata fragmentMetadata = {};
                fragmentMetadata.num_samplers = 1;
                fragmentMetadata.num_storage_buffers = 0;
                fragmentMetadata.num_storage_textures = 0;
                fragmentMetadata.num_uniform_buffers = 1;

                fragmentShader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &fragmentInfo, &fragmentMetadata, 0);
                SDL_free(shaderCode);
            }

            cachedShader._shader = fragmentShader;

            _cachedShaders.insert(std::make_pair(shaderPath, cachedShader));
        }

        return _cachedShaders.find(shaderPath)->second;
    }

    void SDLCreateVertexAttributes(std::vector<SDL_GPUVertexAttribute>& vertexAttributes, const Tbx::BufferLayout& bufferLayout)
    {
        const std::vector<Tbx::BufferElement>& bufferElements = bufferLayout.GetElements();

        vertexAttributes.clear();
        Uint32 offset = 0;

        for (size_t i = 0; i < bufferElements.size(); i++)
        {
            const Tbx::BufferElement& bufferElement = bufferElements[i];
            Tbx::ShaderDataType type = bufferElement.GetType();
            Uint32 size = (Uint32)bufferElement.GetSize();

            vertexAttributes.resize(vertexAttributes.size() + 1);
            SDL_GPUVertexAttribute& vertexAttribute = vertexAttributes.back();

            vertexAttribute.buffer_slot = 0;
            vertexAttribute.location = (Uint32)i;
            switch(type)
            {
                case Tbx::ShaderDataType::Float2:
                    vertexAttribute.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
                    break;
                case Tbx::ShaderDataType::Float3:
                    vertexAttribute.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
                    break;
                case Tbx::ShaderDataType::Float4:
                    vertexAttribute.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
                    break;
                default:
                    SDL_assert(0);
                    break;
            }
            vertexAttribute.offset = offset;

            offset += size;
        }
    }

    void SDLCreateVertexBufferDescriptions(std::vector<SDL_GPUVertexBufferDescription>& vertexBufferDesctiptions, const Tbx::BufferLayout& bufferLayout)
    {
        Uint32 stride = bufferLayout.GetStride();

        vertexBufferDesctiptions.clear();
        vertexBufferDesctiptions.resize(vertexBufferDesctiptions.size() + 1);

        SDL_GPUVertexBufferDescription& vertexBufferDesctiption = vertexBufferDesctiptions.back();
        vertexBufferDesctiption.slot = 0;
        vertexBufferDesctiption.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDesctiption.instance_step_rate = 0;
        vertexBufferDesctiption.pitch = stride;
    }

    SDL_GPUBuffer* SDLCreateBuffer(SDL_GPUDevice* device, const SDL_GPUBufferCreateInfo& bufferCreateInfo)
    {
        SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &bufferCreateInfo);
        return buffer;
    }

    void SDLUploadBuffer(SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer, SDL_GPUBuffer* buffer, Uint32 sourceSize, const void* sourceData)
    {
        SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo = {};
        transferBufferCreateInfo.size = sourceSize;
        transferBufferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);

        void* targetData = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
        SDL_memcpy(targetData, sourceData, sourceSize);
        SDL_UnmapGPUTransferBuffer(device, transferBuffer);

        SDL_GPUTransferBufferLocation transferBufferLocation = {};
        transferBufferLocation.transfer_buffer = transferBuffer;
        transferBufferLocation.offset = 0;

        SDL_GPUBufferRegion bufferRegion = {};
        bufferRegion.buffer = buffer;
        bufferRegion.size = sourceSize;
        bufferRegion.offset = 0;

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
        SDL_UploadToGPUBuffer(copyPass, &transferBufferLocation, &bufferRegion, true);
        SDL_EndGPUCopyPass(copyPass);
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
    }

    SDL_GPUTexture* SDLCreateTexture(SDL_GPUDevice* device, const SDL_GPUTextureCreateInfo& textureCreateInfo)
    {
        SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &textureCreateInfo);
        return texture;
    }

    void SDLUploadTexture(SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer, SDL_GPUTexture* texture, Uint32 textureSize, const void* textureData, Uint32 textureWidth, Uint32 textureHeight)
    {
        SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo = {};
        transferBufferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferBufferCreateInfo.size = textureSize;
        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);

        void* targetData = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
        SDL_memcpy(targetData, textureData, textureSize);
        SDL_UnmapGPUTransferBuffer(device, transferBuffer);

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);

        SDL_GPUTextureTransferInfo textureTransferInfo = {};
        textureTransferInfo.transfer_buffer = transferBuffer;
        textureTransferInfo.offset = 0;

        SDL_GPUTextureRegion textureRegion = {};
        textureRegion.texture = texture;
        textureRegion.w = textureWidth;
        textureRegion.h = textureHeight;
        textureRegion.d = 1;

        SDL_UploadToGPUTexture(copyPass, &textureTransferInfo, &textureRegion, false);
        SDL_EndGPUCopyPass(copyPass);
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
    }

    SDL_Surface* SDLLoadSurface(const char* surfacePath, SDL_PixelFormat pixelFormat)
    {
        SDL_Surface* surface = SDL_LoadBMP(surfacePath);
        if (surface == NULL)
        {
            SDL_Log("Failed to load BMP: %s", SDL_GetError());
            return NULL;
        }
        if (surface->format != pixelFormat)
        {
            SDL_Surface* _surface = SDL_ConvertSurface(surface, pixelFormat);
            SDL_DestroySurface(surface);
            surface = _surface;
        }
        return surface;
    }
}
