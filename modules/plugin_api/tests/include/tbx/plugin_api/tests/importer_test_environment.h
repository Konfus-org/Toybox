#pragma once
#include "tbx/app/application.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/files/tests/in_memory_file_ops.h"
#include "tbx/input/input_manager.h"
#include <filesystem>
#include <string>
#include <vector>

namespace tbx::tests::plugin_api
{
    using InMemoryFileOps = ::tbx::tests::file_system::InMemoryFileOps;

    /// @brief
    /// Purpose: Returns a deterministic absolute-style working directory path for tests on each
    /// platform. Ownership: Returns a value path object with no shared lifetime requirements.
    /// Thread Safety: Thread-safe; no shared mutable state.
    static std::filesystem::path get_test_working_directory()
    {
#if defined(_WIN32)
        return std::filesystem::path("C:/virtual/assets");
#else
        return std::filesystem::path("/virtual/assets");
#endif
    }

    /// @brief
    /// Purpose: Minimal plugin host implementation that exposes engine services needed by importer
    /// tests. Ownership: Owns coordinator, registry, settings, and asset manager for the test
    /// lifetime. Thread Safety: Not thread-safe; intended for single-threaded test setup and
    /// execution.
    class TestPluginHost final : public IPluginHost
    {
      public:
        TestPluginHost(const std::filesystem::path& working_directory)
            : _asset_manager(&_coordinator, working_directory)
            , _settings(_coordinator, true, GraphicsApi::OPEN_GL, {640, 480})
        {
            _settings.paths.working_directory = working_directory;
            _settings.paths.logs_directory = working_directory / "logs";
        }

        const std::string& get_name() const override
        {
            return _name;
        }

        const Handle& get_icon_handle() const override
        {
            return _icon_handle;
        }

        AppSettings& get_settings() override
        {
            return _settings;
        }

        IMessageCoordinator& get_message_coordinator() override
        {
            return _coordinator;
        }

        InputManager& get_input_manager() override
        {
            return _input_manager;
        }

        EntityRegistry& get_entity_registry() override
        {
            return _registry;
        }

        AssetManager& get_asset_manager() override
        {
            return _asset_manager;
        }

        JobSystem& get_job_system() override
        {
            return _job_manager;
        }

        ThreadManager& get_thread_manager() override
        {
            return _thread_manager;
        }

      private:
        std::string _name = "ImporterTests";
        Handle _icon_handle = BoxIcon::HANDLE;
        AppMessageCoordinator _coordinator = {};
        InputManager _input_manager = _coordinator;
        EntityRegistry _registry = {};
        AssetManager _asset_manager;
        AppSettings _settings;
        JobSystem _job_manager;
        ThreadManager _thread_manager;
    };
}
