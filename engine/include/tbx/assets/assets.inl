#pragma once
// Template bodies for the assets module — included by assets.h.

namespace tbx::assets
{
    /// @brief
    /// Purpose: Typed view over find().
    template <typename TAsset>
    std::optional<std::reference_wrapper<TAsset>> find_resident(State& state, const Uuid& id)
    {
        const auto stored = find(state, id);
        if (!stored)
            return {};
        auto* asset = std::any_cast<TAsset>(&stored->get());
        if (!asset)
            return {};
        return *asset;
    }

    template <typename TAsset>
    jobs::Task<Result<std::reference_wrapper<TAsset>>> load(
        State& state,
        events::State& events,
        jobs::State& jobs,
        Handle<TAsset> handle)
    {
        auto resolved = resolve_handle(state, handle.id, handle.path);
        if (!resolved)
            co_return std::unexpected(resolved.error());
        if (auto resident = find_resident<TAsset>(state, resolved->id))
            co_return ok(std::ref(resident->get()));
        if (resolved->relative_path.empty())
            co_return fail("asset {} has no tracked path", resolved->id.to_string());

        // Resolve on this thread so the worker section below touches nothing but the file.
        const auto disk_path = resolve_path(state, resolved->relative_path);
        co_await jobs::on_worker(jobs);
        auto decoded = serialization::deserialize<TAsset>(disk_path);
        co_await jobs::on_main(jobs);
        if (!decoded)
            co_return std::unexpected(decoded.error());
        decoded->id = resolved->id; // a loaded asset knows its own handle
        decoded->path = resolved->relative_path;
        store(
            state,
            events,
            resolved->id,
            resolved->relative_path,
            std::any(std::move(*decoded)));
        if (auto resident = find_resident<TAsset>(state, resolved->id))
            co_return ok(std::ref(resident->get()));
        co_return fail("asset '{}' did not become resident", resolved->relative_path);
    }

    template <typename TAsset>
    Result<std::reference_wrapper<TAsset>> load_now(
        State& state,
        events::State& events,
        Handle<TAsset> handle)
    {
        auto resolved = resolve_handle(state, handle.id, handle.path);
        if (!resolved)
            return std::unexpected(resolved.error());
        if (auto resident = find_resident<TAsset>(state, resolved->id))
            return ok(std::ref(resident->get()));
        if (resolved->relative_path.empty())
            return fail("asset {} has no tracked path", resolved->id.to_string());
        auto decoded = serialization::deserialize<TAsset>(resolve_path(state, resolved->relative_path));
        if (!decoded)
            return std::unexpected(decoded.error());
        decoded->id = resolved->id; // a loaded asset knows its own handle
        decoded->path = resolved->relative_path;
        store(
            state,
            events,
            resolved->id,
            resolved->relative_path,
            std::any(std::move(*decoded)));
        if (auto resident = find_resident<TAsset>(state, resolved->id))
            return ok(std::ref(resident->get()));
        return fail("asset '{}' did not become resident", resolved->relative_path);
    }
}
