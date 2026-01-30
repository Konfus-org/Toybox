#pragma once
#include "tbx/assets/assets.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/texture.h"
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"
#include <filesystem>

namespace tbx
{
    struct Texture;
    struct Model;
    struct AudioClip;

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
        LoadAssetRequest(const std::filesystem::path& asset_path, TAsset* asset_payload)
            : path(asset_path)
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
            const std::filesystem::path& asset_path,
            Texture* asset_payload,
            TextureWrap wrap,
            TextureFilter filter,
            TextureFormat format)
            : LoadAssetRequest<Texture>(asset_path, asset_payload)
            , wrap(wrap)
            , filter(filter)
            , format(format)
        {
        }

      public:
        TextureWrap wrap = TextureWrap::Repeat;
        TextureFilter filter = TextureFilter::Nearest;
        TextureFormat format = TextureFormat::RGBA;
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
        LoadModelRequest(const std::filesystem::path& asset_path, Model* asset_payload)
            : LoadAssetRequest<Model>(asset_path, asset_payload)
        {
        }
    };

    /// <summary>
    /// Purpose: Message requesting that a shader program payload be loaded.
    /// </summary>
    /// <remarks>
    /// Ownership: The shader program pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access requires external synchronization.
    /// </remarks>
    struct TBX_API LoadShaderProgramRequest : public LoadAssetRequest<ShaderProgram>
    {
      public:
        LoadShaderProgramRequest(
            const std::filesystem::path& asset_path,
            ShaderProgram* asset_payload)
            : LoadAssetRequest<ShaderProgram>(asset_path, asset_payload)
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
        LoadAudioRequest(
            const std::filesystem::path& asset_path,
            AudioClip* asset_payload)
            : LoadAssetRequest<AudioClip>(asset_path, asset_payload)
        {
        }
    };
}
