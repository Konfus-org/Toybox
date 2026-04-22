#pragma once
#include "tbx/core/systems/assets/loaders.h"
#include "tbx/core/systems/audio/clip.h"
#include "tbx/core/systems/graphics/material.h"
#include "tbx/core/systems/graphics/model.h"
#include "tbx/core/systems/graphics/shader.h"
#include "tbx/core/systems/graphics/texture.h"
#include "tbx/core/systems/messaging/message.h"
#include "tbx/core/tbx_api.h"
#include <filesystem>
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: Base message requesting that an asset payload be loaded.
    /// @details
    /// Ownership: The asset pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access requires external synchronization.
    template <typename TAsset>
    struct LoadAssetRequest : public Request<void>
    {
        LoadAssetRequest(std::filesystem::path asset_path, TAsset* asset_payload);

        std::filesystem::path path;
        TAsset* asset = nullptr;
    };

    /// @brief
    /// Purpose: Message requesting that a texture payload be loaded with specific settings.
    /// @details
    /// Ownership: The texture asset pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access requires external synchronization.
    struct TBX_API LoadTextureRequest : public LoadAssetRequest<Texture>
    {
        LoadTextureRequest(
            std::filesystem::path asset_path,
            Texture* asset_payload,
            Texture texture)
            : LoadAssetRequest<Texture>(std::move(asset_path), asset_payload)
            , texture(std::move(texture))
        {
        }

        Texture texture = {};
    };

    /// @brief
    /// Purpose: Message requesting that a model payload be loaded.
    /// @details
    /// Ownership: The model pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access requires external synchronization.
    struct TBX_API LoadModelRequest : public LoadAssetRequest<Model>
    {
        LoadModelRequest(std::filesystem::path asset_path, Model* asset_payload)
            : LoadAssetRequest<Model>(std::move(asset_path), asset_payload)
        {
        }
    };

    /// @brief
    /// Purpose: Message requesting that a shader payload be loaded.
    /// @details
    /// Ownership: The shader program pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access requires external synchronization.
    struct TBX_API LoadShaderRequest : public LoadAssetRequest<Shader>
    {
        LoadShaderRequest(std::filesystem::path asset_path, Shader* asset_payload)
            : LoadAssetRequest<Shader>(std::move(asset_path), asset_payload)
        {
        }
    };

    /// @brief
    /// Purpose: Message requesting that a material payload be loaded.
    /// @details
    /// Ownership: The material pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access requires external synchronization.
    struct TBX_API LoadMaterialRequest : public LoadAssetRequest<Material>
    {
        LoadMaterialRequest(std::filesystem::path asset_path, Material* asset_payload)
            : LoadAssetRequest<Material>(std::move(asset_path), asset_payload)
        {
        }
    };

    /// @brief
    /// Purpose: Message requesting that an audio clip payload be loaded.
    /// @details
    /// Ownership: The audio clip pointer is non-owning and owned by the caller.
    /// Thread Safety: Payload access requires external synchronization.
    struct TBX_API LoadAudioRequest : public LoadAssetRequest<AudioClip>
    {
        LoadAudioRequest(std::filesystem::path asset_path, AudioClip* asset_payload)
            : LoadAssetRequest<AudioClip>(std::move(asset_path), asset_payload)
        {
        }
    };
}

#include "tbx/core/systems/assets/requests.inl"
