#include "tbx/assets/assets.h"
#include "tbx/assets/messages.h"
#include <string>

namespace tbx
{
    AssetHandle::AssetHandle(const std::filesystem::path& asset_path)
        : _path(asset_path)
    {
    }

    const std::filesystem::path& AssetHandle::get_path() const
    {
        return _path;
    }

    bool AssetHandle::is_ready() const
    {
        return _ready.load();
    }

    void AssetHandle::set_ready(bool ready)
    {
        _ready.store(ready);
    }

    Result dispatcher_missing_result(std::string_view action)
    {
        Result result;
        result.flag_failure(std::string("No global dispatcher available to ").append(action));
        return result;
    }

    std::shared_future<Result> make_missing_dispatcher_future(std::string_view action)
    {
        std::promise<Result> promise;
        promise.set_value(dispatcher_missing_result(action));
        return promise.get_future().share();
    }

    void notify_asset_unload(const std::filesystem::path& path)
    {
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            return;
        }

        UnloadAssetRequest message(path);
        dispatcher->send(message);
    }

    void destroy_asset_handle(AssetHandle* asset) noexcept
    {
        if (asset)
        {
            notify_asset_unload(asset->get_path());
            delete asset;
        }
    }
}
