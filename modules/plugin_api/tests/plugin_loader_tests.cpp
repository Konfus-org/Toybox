#include "pch.h"
#include "tbx/files/filesystem.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_loader.h"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx::tests::plugin_loader
{
    class TestStaticPlugin : public ::tbx::Plugin
    {
      public:
        void on_attach(::tbx::Application&) override {}
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

        std::filesystem::path get_assets_directory() const override
        {
            return assets_directory;
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
    };

    TEST(plugin_loader, loads_static_plugin_from_meta_list)
    {
        ManifestFilesystemOps ops;
        PluginLoader loader;
        auto loaded = loader.load_plugins(std::vector<PluginMeta> {make_static_meta()}, ops);
        ASSERT_EQ(loaded.size(), 1u);
        EXPECT_NE(loaded[0].instance.get(), nullptr);
    }

    TEST(plugin_loader, scans_manifests_using_custom_filesystem)
    {
        ManifestFilesystemOps ops;
        ops.add_directory("virtual");
        ops.add_directory("virtual/logger");
        ops.add_file(
            "virtual/logger/logger.meta",
            R"({
                "name": "TestStaticPlugin",
                "version": "1.0.0",
                "static": true
            })");

        PluginLoader loader;
        auto loaded = loader.load_plugins(
            std::filesystem::path("virtual"),
            std::vector<std::string> {},
            ops);
        ASSERT_EQ(loaded.size(), 1u);
        EXPECT_NE(loaded[0].instance.get(), nullptr);
    }
}
