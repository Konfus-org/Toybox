#pragma once
#include "tbx/assets/assets.h"
#include "tbx/audio/audio_clip.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/model.h"
#include "tbx/graphics/shader.h"
#include "tbx/graphics/texture.h"
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <utility>

namespace tbx
{
    /// <summary>
    /// Purpose: Base message requesting that an asset payload be loaded.
    /// </summary>
    /// <remarks>
    /// Ownership: The asset pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access requires external synchronization.
    /// </remarks>
    template <typename TAsset>
    struct LoadAssetRequest : public Request<void>
    {
      public:
        LoadAssetRequest(std::filesystem::path asset_path, TAsset* asset_payload)
            : path(std::move(asset_path))
            , asset(asset_payload)
        {
        }

      public:
        std::filesystem::path path;
        TAsset* asset = nullptr;
    };

    /// <summary>
    /// Purpose: Message requesting that a texture payload be loaded with specific settings.
    /// </summary>
    /// <remarks>
    /// Ownership: The texture asset pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access requires external synchronization.
    /// </remarks>
    struct TBX_API LoadTextureRequest : public LoadAssetRequest<Texture>
    {
      public:
        LoadTextureRequest(
            std::filesystem::path asset_path,
            Texture* asset_payload,
            TextureWrap wrap,
            TextureFilter filter,
            TextureFormat format,
            TextureMipmaps mipmaps,
            TextureCompression compression)
            : LoadAssetRequest<Texture>(std::move(asset_path), asset_payload)
            , wrap(wrap)
            , filter(filter)
            , format(format)
            , mipmaps(mipmaps)
            , compression(compression)
        {
        }

      public:
        TextureWrap wrap = TextureWrap::REPEAT;
        TextureFilter filter = TextureFilter::LINEAR;
        TextureFormat format = TextureFormat::RGBA;
        TextureMipmaps mipmaps = TextureMipmaps::ENABLED;
        TextureCompression compression = TextureCompression::AUTO;
    };

    /// <summary>
    /// Purpose: Message requesting that a model payload be loaded.
    /// </summary>
    /// <remarks>
    /// Ownership: The model pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access requires external synchronization.
    /// </remarks>
    struct TBX_API LoadModelRequest : public LoadAssetRequest<Model>
    {
      public:
        LoadModelRequest(std::filesystem::path asset_path, Model* asset_payload)
            : LoadAssetRequest<Model>(std::move(asset_path), asset_payload)
        {
        }
    };

    /// <summary>
    /// Purpose: Message requesting that a shader payload be loaded.
    /// </summary>
    /// <remarks>
    /// Ownership: The shader program pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access requires external synchronization.
    /// </remarks>
    struct TBX_API LoadShaderRequest : public LoadAssetRequest<Shader>
    {
      public:
        LoadShaderRequest(std::filesystem::path asset_path, Shader* asset_payload)
            : LoadAssetRequest<Shader>(std::move(asset_path), asset_payload)
        {
        }
    };

    /// <summary>
    /// Purpose: Message requesting that a material payload be loaded.
    /// </summary>
    /// <remarks>
    /// Ownership: The material pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access requires external synchronization.
    /// </remarks>
    struct TBX_API LoadMaterialRequest : public LoadAssetRequest<Material>
    {
      public:
        LoadMaterialRequest(std::filesystem::path asset_path, Material* asset_payload)
            : LoadAssetRequest<Material>(std::move(asset_path), asset_payload)
        {
        }
    };

    /// <summary>
    /// Purpose: Message requesting that an audio clip payload be loaded.
    /// </summary>
    /// <remarks>
    /// Ownership: The audio clip pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access requires external synchronization.
    /// </remarks>
    struct TBX_API LoadAudioRequest : public LoadAssetRequest<AudioClip>
    {
      public:
        LoadAudioRequest(std::filesystem::path asset_path, AudioClip* asset_payload)
            : LoadAssetRequest<AudioClip>(std::move(asset_path), asset_payload)
        {
        }
    };
}
