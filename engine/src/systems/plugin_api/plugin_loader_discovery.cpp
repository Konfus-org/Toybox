#include "plugin_loader_discovery.h"
#include "tbx/utils/string_utils.h"

namespace tbx
{
    std::filesystem::path append_debug_postfix(const std::filesystem::path& library_path)
    {
        const std::string extension = library_path.extension().string();
        if (extension.empty())
            return {};

        const std::string stem = library_path.stem().string();
        if (stem.ends_with("d"))
            return {};

        const std::string debug_name = stem + "d" + extension;
        return library_path.parent_path() / debug_name;
    }

    std::filesystem::path resolve_requested_plugin_library_path(
        const std::filesystem::path& directory,
        const std::string& plugin_name,
        const IFileOps& file_ops)
    {
        const std::string trimmed_name = trim(plugin_name);
        if (trimmed_name.empty())
            return {};

        auto library_path = directory / trimmed_name;
        if (library_path.extension().string().empty())
        {
#if defined(TBX_PLATFORM_WINDOWS)
            library_path.replace_extension(".dll");
#elif defined(TBX_PLATFORM_MACOS)
            if (!trimmed_name.starts_with("lib"))
                library_path = directory / ("lib" + trimmed_name);
            library_path.replace_extension(".dylib");
#else
            if (!trimmed_name.starts_with("lib"))
                library_path = directory / ("lib" + trimmed_name);
            library_path.replace_extension(".so");
#endif
        }

        if (file_ops.exists(library_path))
            return library_path;

        const auto debug_candidate = append_debug_postfix(library_path);
        if (!debug_candidate.empty() && file_ops.exists(debug_candidate))
            return debug_candidate;

        return {};
    }

    std::filesystem::path resolve_requested_plugin_meta_path(
        const std::filesystem::path& directory,
        const std::string& plugin_name,
        const IFileOps& file_ops)
    {
        const auto library_path =
            resolve_requested_plugin_library_path(directory, plugin_name, file_ops);
        if (library_path.empty())
            return {};

        auto meta_path = library_path;
        meta_path += ".meta";
        if (!file_ops.exists(meta_path))
            return {};

        return meta_path;
    }
}
