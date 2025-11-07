#include "pch.h"
#include "tbx/file_system/filesystem_ops.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_loader.h"
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace
{
    class TestStaticPlugin : public ::tbx::Plugin
    {
      public:
        void on_attach(::tbx::Application&) override {}
        void on_detach() override {}
        void on_update(const ::tbx::DeltaTime&) override {}
        void on_message(::tbx::Message&) override {}
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

    class ManifestFilesystemOps : public ::tbx::files::IFilesystemOps
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

        bool exists(const std::filesystem::path& path) const override
        {
            const auto key = path.generic_string();
            return directories.contains(key) || files.contains(key);
        }

        bool is_directory(const std::filesystem::path& path) const override
        {
            return directories.contains(path.generic_string());
        }

        std::vector<::tbx::files::DirectoryEntry>
            recursive_directory_entries(const std::filesystem::path& root) const override
        {
            std::vector<::tbx::files::DirectoryEntry> entries;
            const std::string prefix = root.generic_string();
            for (const auto& [file_path, _] : files)
            {
                if (file_path.rfind(prefix, 0) == 0)
                {
                    ::tbx::files::DirectoryEntry entry;
                    entry.path = file_path;
                    entry.is_regular_file = true;
                    entries.push_back(entry);
                }
            }
            return entries;
        }

        bool read_text_file(const std::filesystem::path& path, std::string& out) const override
        {
            const auto key = path.generic_string();
            auto it = files.find(key);
            if (it == files.end())
            {
                return false;
            }
            out = it->second;
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
    };
}

namespace tbx::tests::plugin_loader
{
    TEST(plugin_loader, loads_static_plugin_from_meta_list)
    {
        auto loaded = ::tbx::load_plugins(std::vector<::tbx::PluginMeta>{make_static_meta()});
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

        auto loaded = ::tbx::load_plugins(std::filesystem::path("virtual"), {}, ops);
        ASSERT_EQ(loaded.size(), 1u);
        EXPECT_NE(loaded[0].instance.get(), nullptr);
    }
}
