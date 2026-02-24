#pragma once
#include "tbx/app/application.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/files/file_ops.h"
#include "tbx/input/input_manager.h"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx::tests::plugin_api
{
    /// <summary>
    /// Purpose: Provides an in-memory implementation of file operations for plugin tests.
    /// Ownership: Owns all staged file contents internally; callers pass paths/data by value and
    /// receive copied data. Thread Safety: Not thread-safe; use on a single thread or protect with
    /// external synchronization.
    /// </summary>
    class InMemoryFileOps final : public IFileOps
    {
      public:
        explicit InMemoryFileOps(std::filesystem::path working_directory)
            : _working_directory(std::move(working_directory))
        {
        }

        std::filesystem::path get_working_directory() const override
        {
            return _working_directory;
        }

        std::filesystem::path resolve(const std::filesystem::path& path) const override
        {
            if (path.is_absolute())
                return path.lexically_normal();
            return (_working_directory / path).lexically_normal();
        }

        bool exists(const std::filesystem::path& path) const override
        {
            return _files.contains(resolve(path));
        }

        FileType get_type(const std::filesystem::path& path) const override
        {
            if (_files.contains(resolve(path)))
                return FileType::FILE;
            return FileType::NONE;
        }

        std::vector<std::filesystem::path> read_directory(
            const std::filesystem::path&) const override
        {
            return {};
        }

        bool read_file(const std::filesystem::path& path, FileDataFormat, std::string& out_data)
            const override
        {
            auto iterator = _files.find(resolve(path));
            if (iterator == _files.end())
                return false;
            out_data = iterator->second;
            return true;
        }

        bool write_file(const std::filesystem::path& path, FileDataFormat, const std::string& data)
            override
        {
            _files[resolve(path)] = data;
            return true;
        }

        void set_text(std::filesystem::path path, std::string data)
        {
            _files[resolve(path)] = std::move(data);
        }

        void set_binary(std::filesystem::path path, const std::vector<unsigned char>& data)
        {
            auto encoded = std::string(
                reinterpret_cast<const char*>(data.data()),
                reinterpret_cast<const char*>(data.data() + data.size()));
            _files[resolve(path)] = std::move(encoded);
        }

      private:
        std::filesystem::path _working_directory = {};
        std::unordered_map<std::filesystem::path, std::string> _files = {};
    };

    /// <summary>
    /// Purpose: Returns a deterministic absolute-style working directory path for tests on each
    /// platform. Ownership: Returns a value path object with no shared lifetime requirements.
    /// Thread Safety: Thread-safe; no shared mutable state.
    /// </summary>
    static std::filesystem::path get_test_working_directory()
    {
#if defined(_WIN32)
        return std::filesystem::path("C:/virtual/assets");
#else
        return std::filesystem::path("/virtual/assets");
#endif
    }

    /// <summary>
    /// Purpose: Minimal plugin host implementation that exposes engine services needed by importer
    /// tests. Ownership: Owns coordinator, registry, settings, and asset manager for the test
    /// lifetime. Thread Safety: Not thread-safe; intended for single-threaded test setup and
    /// execution.
    /// </summary>
    class TestPluginHost final : public IPluginHost
    {
      public:
        explicit TestPluginHost(const std::filesystem::path& working_directory)
            : _asset_manager(working_directory, {}, {}, false)
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

      private:
        std::string _name = "ImporterTests";
        Handle _icon_handle = box_icon;
        AppMessageCoordinator _coordinator = {};
        InputManager _input_manager = _coordinator;
        EntityRegistry _registry = {};
        AssetManager _asset_manager;
        AppSettings _settings;
        JobSystem _job_manager;
    };
}
