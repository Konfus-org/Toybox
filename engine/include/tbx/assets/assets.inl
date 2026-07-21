#pragma once
// Template bodies for Assets — included by assets.h.

namespace tbx
{

    template <typename TAsset>
    Task<Result<std::reference_wrapper<TAsset>>> Assets::load(AssetHandle<TAsset> handle)
    {
        auto resolved = resolve_handle(handle.id, handle.path);
        if (!resolved)
            co_return std::unexpected(resolved.error());
        if (auto resident = find_resident<TAsset>(resolved->id))
            co_return ok(std::ref(resident->get()));
        if (resolved->relative_path.empty())
            co_return fail("asset {} has no tracked path", resolved->id.to_string());

        co_await _jobs.get().on_worker();
        auto decoded = tbx::load<TAsset>(resolve_path(resolved->relative_path));
        co_await _jobs.get().on_main();
        if (!decoded)
            co_return std::unexpected(decoded.error());
        store(resolved->id, resolved->relative_path, std::any(std::move(*decoded)));
        if (auto resident = find_resident<TAsset>(resolved->id))
            co_return ok(std::ref(resident->get()));
        co_return fail("asset '{}' did not become resident", resolved->relative_path);
    }

    template <typename TAsset>
    Result<std::reference_wrapper<TAsset>> Assets::load_now(AssetHandle<TAsset> handle)
    {
        auto resolved = resolve_handle(handle.id, handle.path);
        if (!resolved)
            return std::unexpected(resolved.error());
        if (auto resident = find_resident<TAsset>(resolved->id))
            return ok(std::ref(resident->get()));
        if (resolved->relative_path.empty())
            return fail("asset {} has no tracked path", resolved->id.to_string());
        auto decoded = tbx::load<TAsset>(resolve_path(resolved->relative_path));
        if (!decoded)
            return std::unexpected(decoded.error());
        store(resolved->id, resolved->relative_path, std::any(std::move(*decoded)));
        if (auto resident = find_resident<TAsset>(resolved->id))
            return ok(std::ref(resident->get()));
        return fail("asset '{}' did not become resident", resolved->relative_path);
    }

    template <typename TAsset>
    std::optional<std::reference_wrapper<TAsset>> Assets::find_resident(const Uuid& id)
    {
        const std::scoped_lock lock(_mutex);
        const auto it = _assets.find(id);
        if (it == _assets.end())
            return {};
        auto* asset = std::any_cast<TAsset>(&it->second);
        if (!asset)
            return {};
        _last_access[id] = std::chrono::steady_clock::now(); // referenced: stays resident
        return *asset;
    }
}
