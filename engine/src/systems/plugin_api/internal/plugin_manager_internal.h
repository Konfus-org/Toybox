#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/physics_backend.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/plugin_api/plugin_loader.h"
#include "tbx/systems/plugin_api/plugin_manager.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/string_utils.h"
#include <algorithm>
#include <ctime>
#include <limits>
#include <unordered_set>
#include <utility>

namespace tbx::internal
{
    static constexpr size invalid_plugin_index = std::numeric_limits<size>::max();

    static bool plugin_manager_path_contains_directory_token(
        const std::filesystem::path& path,
        std::string_view directory_name_lowered)
    {
        if (directory_name_lowered.empty())
            return false;

        for (const auto& part : path)
        {
            if (to_lower(part.string()) == directory_name_lowered)
                return true;
        }

        return false;
    }

    static bool plugin_depends_on_name(const LoadedPlugin& plugin, const std::string& lowered_name)
    {
        for (const auto& dependency : plugin.meta.dependencies)
        {
            if (to_lower(trim(dependency)) == lowered_name)
                return true;
        }

        return false;
    }

    static void ensure_physics_service_registered(ServiceProvider& service_provider)
    {
        if (service_provider.has_service<Physics>())
            return;

        auto physics_backend = service_provider.try_get_service<IPhysicsBackend>().lock();
        auto entity_registry = service_provider.get_service<EntityRegistry>().lock();
        auto asset_manager = service_provider.get_service<AssetManager>().lock();
        auto settings = service_provider.get_service<AppSettings>().lock();
        if (!physics_backend || !entity_registry || !asset_manager || !settings)
            return;

        service_provider.register_service<Physics>(std::make_unique<Physics>(
            service_provider.try_get_service<IPhysicsBackend>(),
            service_provider.try_get_service<EntityRegistry>(),
            service_provider.try_get_service<AssetManager>(),
            service_provider.try_get_service<AppSettings>()));
    }

}
