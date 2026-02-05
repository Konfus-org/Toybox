#include "pch.h"
#include "tbx/app/application.h"
#include "tbx/debugging/logging.h"
#include "tbx/files/filesystem.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_loader.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx::tests::plugin_loader
{
    static std::string platform_manifest_path(std::string base_without_extension)
    {
#if defined(TBX_PLATFORM_WINDOWS)
        return base_without_extension + ".dll.meta";
#elif defined(TBX_PLATFORM_MACOS)
        return base_without_extension + ".dylib.meta";
#else
        return base_without_extension + ".so.meta";
#endif
    }

    class TestStaticPlugin : public ::tbx::Plugin
    {
      public:
        void on_attach(::tbx::IPluginHost&) override {}
        void on_detach() override {}
        void on_update(const ::tbx::DeltaTime&) override {}
        void on_recieve_message(::tbx::Message&) override {}
    };

    TBX_REGISTER_STATIC_PLUGIN(TestStaticPlugin, TestStaticPlugin)

    ::tbx::PluginMeta make_static_meta()
    {
        ::tbx::PluginMeta meta;
        meta.name = "TestStaticPlugin";
        meta.version = "1.0.0";
        meta.abi_version = ::tbx::PluginAbiVersion;
        meta.linkage = ::tbx::PluginLinkage::Static;
        return meta;
    }

    class ManifestFilesystemOps : public ::tbx::IFileSystem
    {
      public:
        void add_directory(std::string path)
        {
            directories.insert(std::move(path));
        }

        void add_file(std::string path, std::string data)
        {
            files.emplace(std::move(path), std::move(data));
        }

        std::filesystem::path get_working_directory() const override
        {
            return working_directory;
        }

        std::filesystem::path get_plugins_directory() const override
        {
            return plugins_directory;
        }

        std::filesystem::path get_logs_directory() const override
        {
            return logs_directory;
        }

        std::vector<std::filesystem::path> get_assets_directories() const override
        {
            std::vector<std::filesystem::path> roots = assets_directories;
            if (!assets_directory.empty())
            {
                bool has_primary = false;
                for (const auto& root : roots)
                {
                    if (root == assets_directory)
                    {
                        has_primary = true;
                        break;
                    }
                }
                if (!has_primary)
                {
                    roots.insert(roots.begin(), assets_directory);
                }
            }
            return roots;
        }

        void add_assets_directory(const std::filesystem::path& path) override
        {
            const auto resolved = resolve_relative_path(path);
            if (assets_directory.empty())
            {
                assets_directory = resolved;
            }
            assets_directories.push_back(resolved);
        }

        std::filesystem::path resolve_asset_path(
            const std::filesystem::path& path) const override
        {
            if (path.empty())
            {
                return {};
            }
            if (path.is_absolute())
            {
                return path;
            }
            const auto roots = get_assets_directories();
            for (const auto& root : roots)
            {
                if (root.empty())
                {
                    continue;
                }
                const auto candidate = resolve_relative_path(root / path);
                if (exists(candidate))
                {
                    return candidate;
                }
            }
            if (!assets_directory.empty())
            {
                return resolve_relative_path(assets_directory / path);
            }
            return resolve_relative_path(path);
        }

        std::filesystem::path resolve_relative_path(
            const std::filesystem::path& path) const override
        {
            if (path.is_absolute() || working_directory.empty())
                return path;
            return working_directory / path;
        }

        bool exists(const std::filesystem::path& path) const override
        {
            const auto key = resolve_relative_path(path).generic_string();
            return directories.contains(key) || files.find(key) != files.end();
        }

        FilePathType get_file_type(const std::filesystem::path& path) const override
        {
            const auto key = resolve_relative_path(path).generic_string();
            if (directories.contains(key))
                return FilePathType::Directory;
            if (files.find(key) != files.end())
                return FilePathType::Regular;
            return FilePathType::None;
        }

        std::vector<std::filesystem::path> read_directory(
            const std::filesystem::path& root) const override
        {
            std::vector<std::filesystem::path> entries;
            const std::string prefix = resolve_relative_path(root).generic_string();
            for (const auto& dir : directories)
            {
                if (dir.starts_with(prefix))
                {
                    entries.push_back(dir);
                }
            }
            for (const auto& [file_path, _] : files)
            {
                if (file_path.starts_with(prefix))
                {
                    entries.push_back(file_path);
                }
            }
            return entries;
        }

        bool create_directory(const std::filesystem::path&) override
        {
            return true;
        }

        bool create_file(const std::filesystem::path& path) override
        {
            files[resolve_relative_path(path).generic_string()] = "";
            return true;
        }

        bool read_file(
            const std::filesystem::path& path,
            FileDataFormat,
            std::string& out) const override
        {
            const auto key = resolve_relative_path(path).generic_string();
            auto it = files.find(key);
            if (it == files.end())
                return false;
            out = it->second;
            return true;
        }

        bool write_file(
            const std::filesystem::path& path,
            FileDataFormat,
            const std::string& data) override
        {
            files[resolve_relative_path(path).generic_string()] = data;
            return true;
        }

        bool remove(const std::filesystem::path&) override
        {
            return true;
        }

        bool rename(const std::filesystem::path&, const std::filesystem::path&) override
        {
            return true;
        }

        bool copy(const std::filesystem::path&, const std::filesystem::path&) override
        {
            return true;
        }

        std::unordered_set<std::string> directories;
        std::unordered_map<std::string, std::string> files;
        std::filesystem::path working_directory;
        std::filesystem::path plugins_directory;
        std::filesystem::path logs_directory;
        std::filesystem::path assets_directory;
        std::vector<std::filesystem::path> assets_directories;
    };

    class TestPluginHost final : public ::tbx::IPluginHost
    {
      public:
        explicit TestPluginHost(::tbx::IFileSystem& filesystem)
            : _filesystem(filesystem)
            , _asset_manager(_filesystem)
            , _settings(_coordinator, true, ::tbx::GraphicsApi::OpenGL, {1280, 720})
        {
        }

        const std::string& get_name() const override
        {
            return _name;
        }

        ::tbx::AppSettings& get_settings() override
        {
            return _settings;
        }

        ::tbx::IMessageCoordinator& get_message_coordinator() override
        {
            return _coordinator;
        }

        ::tbx::IFileSystem& get_filesystem() override
        {
            return _filesystem;
        }

        ::tbx::EntityRegistry& get_entity_registry() override
        {
            return _entity_manager;
        }

        ::tbx::AssetManager& get_asset_manager() override
        {
            return _asset_manager;
        }

      private:
        std::string _name = "PluginLoaderTests";
        ::tbx::AppMessageCoordinator _coordinator = {};
        ::tbx::IFileSystem& _filesystem;
        ::tbx::EntityRegistry _entity_manager = {};
        ::tbx::AssetManager _asset_manager;
        ::tbx::AppSettings _settings;
    };

    TEST(plugin_loader, loads_static_plugin_from_meta_list)
    {
        ManifestFilesystemOps ops;
        TestPluginHost host(ops);
        auto loaded = ::tbx::load_plugins(std::vector<PluginMeta> {make_static_meta()}, ops, host);
        const bool has_fallback_logger =
            ::tbx::PluginRegistry::get_instance().find_plugin(
                std::string(get_stdout_fallback_logger_name()))
            != nullptr;
        const size_t expected_count = has_fallback_logger ? 2u : 1u;
        ASSERT_EQ(loaded.size(), expected_count);
        auto found_test = std::find_if(
            loaded.begin(),
            loaded.end(),
            [](const LoadedPlugin& plugin)
            {
                return plugin.meta.name == "TestStaticPlugin";
            });
        ASSERT_NE(found_test, loaded.end());
        EXPECT_NE(found_test->instance.get(), nullptr);

        // Fallback logging plugin loads only when the static plugin is registered in this binary.
        if (has_fallback_logger)
        {
            auto found_fallback = std::find_if(
                loaded.begin(),
                loaded.end(),
                [](const LoadedPlugin& plugin)
                {
                    return plugin.meta.name == get_stdout_fallback_logger_name();
                });
            ASSERT_NE(found_fallback, loaded.end());
        }
    }

    TEST(plugin_loader, scans_manifests_using_custom_filesystem)
    {
        ManifestFilesystemOps ops;
        TestPluginHost host(ops);
        ops.add_directory("virtual");
        ops.add_directory("virtual/logger");
        ops.add_file(
            platform_manifest_path("virtual/logger/logger"),
            R"({
                "name": "TestStaticPlugin",
                "version": "1.0.0",
                "abi_version": 1,
                "static": true
            })");

        auto loaded = ::tbx::load_plugins(
            std::filesystem::path("virtual"),
            std::vector<std::string> {},
            ops,
            host);
        const bool has_fallback_logger =
            ::tbx::PluginRegistry::get_instance().find_plugin(
                std::string(get_stdout_fallback_logger_name()))
            != nullptr;
        const size_t expected_count = has_fallback_logger ? 2u : 1u;
        ASSERT_EQ(loaded.size(), expected_count);
        auto found_test = std::find_if(
            loaded.begin(),
            loaded.end(),
            [](const LoadedPlugin& plugin)
            {
                return plugin.meta.name == "TestStaticPlugin";
            });
        ASSERT_NE(found_test, loaded.end());
        EXPECT_NE(found_test->instance.get(), nullptr);
        if (has_fallback_logger)
        {
            auto found_fallback = std::find_if(
                loaded.begin(),
                loaded.end(),
                [](const LoadedPlugin& plugin)
                {
                    return plugin.meta.name == get_stdout_fallback_logger_name();
                });
            ASSERT_NE(found_fallback, loaded.end());
        }
    }

    TEST(plugin_loader, rejects_plugin_with_mismatched_abi_version)
    {
        ManifestFilesystemOps ops;
        TestPluginHost host(ops);
        ops.add_directory("virtual");
        ops.add_directory("virtual/logger");
        ops.add_file(
            platform_manifest_path("virtual/logger/logger"),
            R"({
                "name": "TestStaticPlugin",
                "version": "1.0.0",
                "abi_version": 77,
                "static": true
            })");

        auto loaded = ::tbx::load_plugins(
            std::filesystem::path("virtual"),
            std::vector<std::string> {},
            ops,
            host);
        const bool has_fallback_logger =
            ::tbx::PluginRegistry::get_instance().find_plugin(
                std::string(get_stdout_fallback_logger_name()))
            != nullptr;
        const size_t expected_count = has_fallback_logger ? 1u : 0u;
        ASSERT_EQ(loaded.size(), expected_count);
        if (has_fallback_logger)
        {
            EXPECT_EQ(loaded[0].meta.name, get_stdout_fallback_logger_name());
        }
    }
}
