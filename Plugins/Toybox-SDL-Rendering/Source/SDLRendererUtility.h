#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <map>
#include <Tbx/Graphics/Buffers.h>
#include <Tbx/Graphics/Material.h>

namespace SDLRendering
{
    struct SDLCachedTexture
    {
        SDL_GPUTexture* _texture;
        SDL_GPUSampler* _sampler;

        SDLCachedTexture();
        void Release(SDL_GPUDevice* device);
    };

    struct SDLCachedTextureManager
    {
        std::map<const std::string, SDLCachedTexture> _cachedTextures;

        void Release(SDL_GPUDevice* device);
        SDLCachedTexture Add(SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer, const Tbx::Texture& texture);
        SDLCachedTexture Get(const Tbx::Texture& texture);
    };

    struct SDLCachedShader
    {
        SDL_GPUShader* _shader;

        SDLCachedShader();
        void Release(SDL_GPUDevice* device);
    };

    struct SDLCachedShaderManager
    {
        std::map<const std::string, SDLCachedShader> _cachedShaders;

        void Release(SDL_GPUDevice* device);
        SDLCachedShader AddVertexShader(SDL_GPUDevice* device, const Tbx::Shader& shader);
        SDLCachedShader AddFragmentShader(SDL_GPUDevice* device, const Tbx::Shader& shader);
    };

    void SDLCreateVertexAttributes(std::vector<SDL_GPUVertexAttribute>& vertexAttributes, const Tbx::BufferLayout& bufferLayout);

    void SDLCreateVertexBufferDescriptions(std::vector<SDL_GPUVertexBufferDescription>& vertexBufferDesctiptions, const Tbx::BufferLayout& bufferLayout);

    SDL_GPUBuffer* SDLCreateBuffer(SDL_GPUDevice* device, const SDL_GPUBufferCreateInfo& bufferCreateInfo);

    void SDLUploadBuffer(SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer, SDL_GPUBuffer* buffer, Uint32 sourceSize, const void* sourceData);

    SDL_GPUTexture* SDLCreateTexture(SDL_GPUDevice* device, const SDL_GPUTextureCreateInfo& textureCreateInfo);

    void SDLUploadTexture(SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer, SDL_GPUTexture* texture, Uint32 textureSize, const void* textureData, Uint32 textureWidth, Uint32 textureHeight);

    SDL_Surface* SDLLoadSurface(const char* surfacePath, SDL_PixelFormat pixelFormat);
}
