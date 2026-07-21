#pragma once
// Template bodies for Assets — included by assets.h.

namespace tbx
{
    template <typename TAsset>
    Result<std::reference_wrapper<TAsset>> Assets::acquire(AssetHandle<TAsset> handle)
    {
        if (!handle.is_valid())
            return fail("cannot acquire: the handle is nil");
        if (auto resident = get(handle))
            return ok(std::ref(resident->get()));
        const auto relative_path = find_relative_path(handle.id);
        if (!relative_path)
            return fail("asset {} has no tracked path (load it once first)", handle.id.to_string());
        auto loaded = load_now<TAsset>(*relative_path);
        if (!loaded)
            return std::unexpected(loaded.error());
        if (auto resident = get(*loaded))
            return ok(std::ref(resident->get()));
        return fail("asset '{}' did not become resident", *relative_path);
    }

    template <typename TAsset>
    std::optional<std::reference_wrapper<TAsset>> Assets::get(AssetHandle<TAsset> handle)
    {
        const std::scoped_lock lock(_mutex);
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
        {
            const std::scoped_lock lock(_mutex);
            if (_assets.contains(id))
                co_return AssetHandle<TAsset> {.id = id};
        }

        co_await _jobs.get().on_worker();
        auto decoded = decode<TAsset>(resolve_path(relative_path));
        co_await _jobs.get().on_main();
        if (!decoded)
            co_return std::unexpected(decoded.error());
        store(id, relative_path, std::any(std::move(*decoded)));
        co_return AssetHandle<TAsset> {.id = id};
    }

    template <typename TAsset>
    Result<AssetHandle<TAsset>> Assets::load_now(const std::string& relative_path)
    {
        auto prepared = prepare(relative_path);
        if (!prepared)
            return std::unexpected(prepared.error());
        const Uuid id = *prepared;
        {
            const std::scoped_lock lock(_mutex);
            if (_assets.contains(id))
                return ok(AssetHandle<TAsset> {.id = id});
        }
        auto decoded = decode<TAsset>(resolve_path(relative_path));
        if (!decoded)
            return std::unexpected(decoded.error());
        store(id, relative_path, std::any(std::move(*decoded)));
        return ok(AssetHandle<TAsset> {.id = id});
    }
}
