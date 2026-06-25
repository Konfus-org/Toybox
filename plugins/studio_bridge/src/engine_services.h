#pragma once
#include "tbx/interfaces/rpc_host.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/graphics/gizmos.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/input/input_manager.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/scripting/script_system.h"
#include "tbx/systems/scripting/scripting_registry.h"
#include "tbx/systems/world/manager.h"
#include <memory>
#include <string>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The set of engine services the bridge borrows, resolved once from the application
    /// on ApplicationInitializedEvent and shared (by reference) with every bridge subsystem.
    /// @details
    /// Ownership: Holds non-owning weak_ptrs to engine-owned services plus a borrowed pointer to
    /// the app's graphics settings (valid for the app lifetime). Thread Safety: Read from the main
    /// thread; lock() the weak_ptrs at point of use.
    struct EngineServices
    {
        std::weak_ptr<tbx::WorldManager> world_manager = {};
        std::weak_ptr<tbx::Rendering> rendering = {};
        std::weak_ptr<tbx::AssetManager> asset_manager = {};
        std::weak_ptr<tbx::InputManager> input_manager = {};
        std::weak_ptr<tbx::Gizmos> gizmos = {};
        std::weak_ptr<tbx::ScriptingRegistry> scripting_registry = {};
        // The simulation systems, reset when leaving play mode so no body/velocity or per-script
        // runtime state survives the restore of the pre-play world snapshot.
        std::weak_ptr<tbx::Physics> physics = {};
        std::weak_ptr<tbx::ScriptSystem> script_system = {};

        // The RPC transport (published by the WindowsRPC plugin) the subsystems push notifications
        // through. Set by the bridge once bound; subsystems lock it at the point of use.
        std::weak_ptr<tbx::IRpcHost> rpc_host = {};
        const tbx::GraphicsSettings* graphics_settings = nullptr;
        std::string app_name = {};

        /// @brief The active world, or nullptr when no world manager / active world exists.
        std::shared_ptr<tbx::World> active_world() const
        {
            auto manager = world_manager.lock();
            return manager ? manager->get_active_world().lock() : nullptr;
        }
    };
}
