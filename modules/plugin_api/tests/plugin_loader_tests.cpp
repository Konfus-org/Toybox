#include "pch.h"
#include "tbx/file_system/filesystem.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_loader.h"
#include <unordered_map>
#include <unordered_set>

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
        meta.type = "plugin";
        meta.linkage = ::tbx::PluginLinkage::Static;
        return meta;
    }

    class ManifestFilesystemOps : public ::tbx::IFileSystem
    {
      public:
        void add_directory(String path)
        {
            directories.insert(std::move(path));
        }

        void add_file(String path, String data)
        {
            files.emplace(std::move(path), std::move(data));
        }

        FilePath get_working_directory() const override { return working_directory; }

        FilePath get_plugins_directory() const override { return plugins_directory; }

        FilePath get_logs_directory() const override { return logs_directory; }

        FilePath get_assets_directory() const override { return assets_directory; }

        FilePath resolve_relative_path(const FilePath& path) const override
        {
            if (path.std_path().is_absolute() || working_directory.empty())
                return path;
            return FilePath(working_directory.std_path() / path.std_path());
        }

        bool exists(const FilePath& path) const override
        {
            const auto key = resolve_relative_path(path).std_path().generic_string();
            return directories.contains(key) || files.contains(key);
        }

        FilePathType get_file_type(const FilePath& path) const override
        {
            const auto key = resolve_relative_path(path).std_path().generic_string();
            if (directories.contains(key))
                return FilePathType::Directory;
            if (files.contains(key))
                return FilePathType::Regular;
            return FilePathType::None;
        }

        List<FilePath> read_directory(const FilePath& root) const override
        {
            List<FilePath> entries;
            const String prefix = resolve_relative_path(root).std_path().generic_string();
            for (const auto& dir : directories)
            {
                if (dir.starts_with(prefix))
                {
                    entries.push_back(std::filesystem::path(dir.std_str()));
                }
            }
            for (const auto& [file_path, _] : files)
            {
                if (file_path.starts_with(prefix))
                {
                    entries.push_back(std::filesystem::path(file_path.std_str()));
                }
            }
            return entries;
        }

        bool create_directory(const FilePath&) override { return true; }

        bool create_file(const FilePath& path) override
        {
            files[resolve_relative_path(path).std_path().generic_string()] = "";
            return true;
        }

        bool read_file(const FilePath& path, String& out, FileDataFormat) const override
        {
            const auto key = resolve_relative_path(path).std_path().generic_string();
            auto it = files.find(key);
            if (it == files.end())
                return false;
            out = it->second;
            return true;
        }

        bool write_file(const FilePath& path, const String& data, FileDataFormat) override
        {
            files[resolve_relative_path(path).std_path().generic_string()] = data;
            return true;
        }

        bool remove(const FilePath&) override { return true; }

        bool rename(const FilePath&, const FilePath&) override { return true; }

        bool copy(const FilePath&, const FilePath&) override { return true; }

        std::unordered_set<String> directories;
        std::unordered_map<String, String> files;
        FilePath working_directory;
        FilePath plugins_directory;
        FilePath logs_directory;
        FilePath assets_directory;
    };
    
    TEST(plugin_loader, loads_static_plugin_from_meta_list)
    {
        ManifestFilesystemOps ops;
        auto loaded = load_plugins(List<PluginMeta>{make_static_meta()}, ops);
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
                "type": "plugin",
                "static": true
            })");

        auto loaded = load_plugins(FilePath("virtual"), {}, ops);
        ASSERT_EQ(loaded.size(), 1u);
        EXPECT_NE(loaded[0].instance.get(), nullptr);
    }
}
