#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/plugin_api/loaded_plugin.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    class TBX_API PluginLoader final
    {
      public:
        PluginLoader() = default;
        ~PluginLoader() noexcept = default;

      public:
        PluginLoader(const PluginLoader&) = delete;
        PluginLoader& operator=(const PluginLoader&) = delete;
        PluginLoader(PluginLoader&&) = delete;
        PluginLoader& operator=(PluginLoader&&) = delete;

      public:
        LoadedPlugins load(
            const std::filesystem::path& path,
            const std::vector<std::string>& requested_ids = {});

        LoadedPlugins load(
            const std::filesystem::path& path,
            const std::vector<std::string>& requested_ids,
            IFileOps& file_ops);
    };
}
