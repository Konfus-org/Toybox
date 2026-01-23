#pragma once
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <memory>

namespace tbx
{
    class AssetHandle;

    /// <summary>
    /// Purpose: Message requesting that an asset be loaded into the provided handle.
    /// </summary>
    /// <remarks>
    /// Ownership: The asset handle is shared between the caller and the message handler.
    /// Thread Safety: Message dispatch follows the dispatcher contract; asset mutations must
    /// be synchronized through Asset accessors.
    /// </remarks>
    struct TBX_API LoadAssetRequest : public Request<void>
    {
        LoadAssetRequest(
            const std::filesystem::path& asset_path,
            const std::shared_ptr<AssetHandle>& asset_handle);

        std::filesystem::path path;
        std::shared_ptr<AssetHandle> asset;
    };

    /// <summary>
    /// Purpose: Message informing the system that the last reference to an asset was released.
    /// </summary>
    /// <remarks>
    /// Ownership: The asset resource is no longer owned once this message is emitted.
    /// Thread Safety: Message dispatch follows the dispatcher contract.
    /// </remarks>
    struct TBX_API UnloadAssetRequest : public Request<void>
    {
        UnloadAssetRequest(const std::filesystem::path& asset_path);

        std::filesystem::path path;
    };
}
