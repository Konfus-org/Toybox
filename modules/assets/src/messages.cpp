#include "tbx/assets/messages.h"
#include "tbx/assets/assets.h"

namespace tbx
{
    LoadAssetRequest::LoadAssetRequest(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<AssetHandle>& asset_handle)
        : path(asset_path)
        , asset(asset_handle)
    {
    }

    UnloadAssetRequest::UnloadAssetRequest(const std::filesystem::path& asset_path)
        : path(asset_path)
    {
    }
}
