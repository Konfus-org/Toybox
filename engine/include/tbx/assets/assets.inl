#pragma once
// Template bodies for Assets — included by assets.h.

namespace tbx
{
    template <typename TAsset>
    std::optional<std::reference_wrapper<TAsset>> Assets::get(AssetHandle<TAsset> handle)
    {
        const auto it = _assets.find(handle.id);
        if (it == _assets.end())
            return {};
        auto* asset = std::any_cast<TAsset>(&it->second);
        if (!asset)
            return {};
        return *asset;
    }

    template <typename TAsset>
    Task<Result<AssetHandle<TAsset>>> Assets::load(std::string relative_path)
    {
        auto prepared = prepare(relative_path);
        if (!prepared)
            co_return std::unexpected(prepared.error());
        const Uuid id = *prepared;
        if (_assets.contains(id))
            co_return AssetHandle<TAsset> {.id = id};

        co_await _jobs.get().on_worker();
        auto decoded = decode<TAsset>(_root / relative_path);
        co_await _jobs.get().on_main();
        if (!decoded)
            co_return std::unexpected(decoded.error());
        store(id, relative_path, std::any(std::move(*decoded)));
        co_return AssetHandle<TAsset> {.id = id};
    }
}
