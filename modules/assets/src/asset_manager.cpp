#include "tbx/assets/asset_manager.h"
#include "tbx/files/file_operator.h"
#include <algorithm>
#include <filesystem>

namespace tbx
{
    static std::filesystem::path get_resources_directory()
    {
#if defined(TBX_RESOURCES)
        auto resources = std::filesystem::path(TBX_RESOURCES);
        return resources.lexically_normal();
#endif
        return {};
    }

    AssetManager::AssetManager(
        std::filesystem::path working_directory,
        std::vector<std::filesystem::path> asset_directories,
        AssetMetaSource meta_source,
        bool include_default_resources)
        : _working_directory(std::move(working_directory))
        , _meta_source(std::move(meta_source))
    {
        FileOperator file_operator = FileOperator(_working_directory);
        _working_directory = file_operator.get_working_directory();

        if (include_default_resources)
        {
#if defined(TBX_RELEASE)
            // Release flattens all assets into one resources dir
            add_asset_directory(file_operator.resolve("resources"));
#else
            // Debug has seperate dirs for ease of debugging and hot reloading
            auto resources = get_resources_directory();
            if (!resources.empty())
                add_asset_directory(resources);
            for (const auto& directory : asset_directories)
                add_asset_directory(directory);
#endif
        }

        discover_assets();
    }

    void AssetManager::unload_all()
    {
        TBX_TRACE_INFO("Unloading all assets.");
        std::lock_guard lock(_mutex);
        _stores.clear();
    }

    void AssetManager::unload_unreferenced()
    {
        std::lock_guard lock(_mutex);
        for (const auto& store : _stores)
        {
            store.second->unload_unreferenced();
        }
    }

    Uuid AssetManager::read_meta_uuid(const std::filesystem::path& asset_path) const
    {
        if (_meta_source)
        {
            AssetMeta meta = {};
            if (_meta_source(asset_path, meta))
                return meta.id;
            return {};
        }

        AssetMeta meta = {};
        if (!_meta_reader.try_parse_from_disk(_working_directory, asset_path, meta))
            return {};
        return meta.id;
    }

    void AssetManager::discover_assets()
    {
        FileOperator file_operator = FileOperator(_working_directory);
        for (const auto& root : _asset_directories)
        {
            if (root.empty())
            {
                continue;
            }
            auto entries = file_operator.read_directory(root);
            for (const auto& entry : entries)
            {
                if (file_operator.get_type(entry) != FileType::FILE)
                {
                    continue;
                }
                if (entry.extension() == ".meta")
                {
                    continue;
                }
                auto normalized = normalize_path(entry);
                if (_pool.contains(normalized.path_key))
                {
                    continue;
                }
                get_or_create_registry_entry(normalized);
            }
        }
    }

    void AssetManager::set_pinned(const Handle& handle, bool is_pinned)
    {
        std::lock_guard lock(_mutex);
        auto* entry = get_or_create_registry_entry(handle);
        if (!entry)
            return;

        for (auto& store : _stores)
            store.second->set_pinned(entry->path_key, is_pinned);
    }

    void AssetManager::add_asset_directory(const std::filesystem::path& path)
    {
        if (path.empty())
            return;

        FileOperator file_operator = FileOperator(_working_directory);
        std::filesystem::path resolved = file_operator.resolve(path);
        if (resolved.empty())
            return;

        std::lock_guard lock(_mutex);
        bool is_duplicate = std::any_of(
            _asset_directories.begin(),
            _asset_directories.end(),
            [&resolved](const std::filesystem::path& existing)
            {
                return existing == resolved;
            });
        if (is_duplicate)
            return;

        _asset_directories.push_back(resolved);
        discover_assets();
    }

    std::vector<std::filesystem::path> AssetManager::get_asset_directories() const
    {
        std::lock_guard lock(_mutex);
        return _asset_directories;
    }

    Uuid AssetManager::resolve_asset_id(const Handle& handle)
    {
        std::lock_guard lock(_mutex);
        auto* entry = get_or_create_registry_entry(handle);
        if (!entry)
        {
            return handle.id;
        }
        return entry->id;
    }

    std::filesystem::path AssetManager::resolve_asset_path(
        const std::filesystem::path& asset_path) const
    {
        std::lock_guard lock(_mutex);
        return resolve_asset_path_no_lock(asset_path);
    }

    std::filesystem::path AssetManager::resolve_asset_path_no_lock(
        const std::filesystem::path& asset_path) const
    {
        if (asset_path.empty())
            return asset_path;
        if (asset_path.is_absolute())
            return asset_path;

        FileOperator file_operator = FileOperator(_working_directory);
        for (const auto& root : _asset_directories)
        {
            if (root.empty())
                continue;

            auto candidate = file_operator.resolve(root / asset_path);
            if (file_operator.exists(candidate))
                return candidate;
        }

        return file_operator.resolve(asset_path);
    }
}
