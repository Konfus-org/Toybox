#pragma once

namespace tbx
{
    template <typename TAsset>
    LoadAssetRequest<TAsset>::LoadAssetRequest(
        std::filesystem::path asset_path,
        TAsset* asset_payload)
        : path(std::move(asset_path))
        , asset(asset_payload)
    {
    }
}
