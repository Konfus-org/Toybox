#include "tbx/assets/asset_manager.h"
#include "tbx/common/string_utils.h"
#include "tbx/files/file_ops.h"
#include <algorithm>
#include <filesystem>
#include <string>

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
        HandleSource handle_source,
        bool include_default_resources,
        std::unique_ptr<IAssetHandleSerializer> asset_handle_serializer)
        : _working_directory(std::move(working_directory))
        , _asset_handle_serializer(std::move(asset_handle_serializer))
        , _handle_source(std::move(handle_source))
    {
        if (!_asset_handle_serializer)
            _asset_handle_serializer = std::make_unique<AssetHandleSerializer>();

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

    bool AssetManager::write_meta_with_id(const std::filesystem::path& asset_path, Uuid id) const
    {
        auto file_operator = FileOperator(_working_directory);
        auto handle = Handle(id);
        return _asset_handle_serializer && _asset_handle_serializer->try_write_to_disk(file_operator, asset_path, handle);
    }

    Uuid AssetManager::generate_unique_asset_id_locked() const
    {
        auto generated = Uuid::generate();
        while (!generated.is_valid() || _registry_by_id.contains(generated))
            generated = Uuid::generate();
        return generated;
    }

    AssetManager::ResolvedAssetMetaId AssetManager::resolve_or_repair_asset_id(
        const NormalizedAssetPath& normalized)
    {
        auto file_operator = FileOperator(_working_directory);
        if (!file_operator.exists(normalized.resolved_path))
        {
            TBX_TRACE_WARNING(
                "AssetManager: requested asset '{}' was not found on disk. Using runtime id only.",
                normalized.normalized_path);
            return {};
        }

        if (_meta_source)
        {
            auto handle = Handle();
            if (!_handle_source(normalized.resolved_path, handle))
                return {};

            auto result = ResolvedAssetMetaId();
            result.resolved_id = handle.id;
            result.state = handle.id.is_valid() ? AssetMetaState::VALID : AssetMetaState::INVALID;
            return result;
        }

        auto result = ResolvedAssetMetaId();
        auto meta_path = normalized.resolved_path;
        meta_path += ".meta";

        if (file_operator.exists(meta_path)
            && _meta_reader.try_parse_from_disk(_working_directory, normalized.resolved_path, meta)
            && meta.id.is_valid())
        {
            auto parsed_handle = _asset_handle_serializer->read_from_disk(
                _working_directory,
                normalized.resolved_path);
            if (parsed_handle && parsed_handle->id.is_valid())
            {
                result.resolved_id = parsed_handle->id;
                result.state = AssetMetaState::VALID;
                return result;
            }
        }

        result.state =
            file_operator.exists(meta_path) ? AssetMetaState::INVALID : AssetMetaState::MISSING;
        result.resolved_id = generate_unique_asset_id_locked();
        auto write_success = write_meta_with_id(normalized.resolved_path, result.resolved_id);
        if (result.state == AssetMetaState::MISSING)
        {
            TBX_TRACE_WARNING(
                "AssetManager: missing metadata for asset '{}'. Generated '{}' with id={}.",
                normalized.normalized_path,
                meta_path.generic_string(),
                to_string(result.resolved_id));
        }
        else
        {
            TBX_TRACE_WARNING(
                "AssetManager: invalid metadata for asset '{}'. Rewrote '{}' with id={}.",
                normalized.normalized_path,
                meta_path.generic_string(),
                to_string(result.resolved_id));
        }

        if (!write_success)
        {
            TBX_TRACE_WARNING(
                "AssetManager: failed to write metadata sidecar '{}' for asset '{}'. Using id={} "
                "for this session.",
                meta_path.generic_string(),
                normalized.normalized_path,
                to_string(result.resolved_id));
        }

        return result;
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
            auto asset_entries = std::vector<std::filesystem::path>();
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
                asset_entries.push_back(entry);
            }

            std::sort(
                asset_entries.begin(),
                asset_entries.end(),
                [](const std::filesystem::path& left, const std::filesystem::path& right)
                {
                    return left.lexically_normal().generic_string()
                           < right.lexically_normal().generic_string();
                });

            for (const auto& entry : asset_entries)
            {
                auto normalized = normalize_path(entry);
                if (_pool.contains(normalized.path_key))
                {
                    continue;
                }
                static_cast<void>(get_or_create_registry_entry(normalized));
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
            if (handle.name.empty())
                return handle.id;
            return {};
        }
        return entry->id;
    }

    std::shared_ptr<Texture> AssetManager::load(
        const Handle& handle,
        const TextureLoadParameters& parameters)
    {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(_mutex);
        auto* entry = get_or_create_registry_entry(handle);
        if (!entry)
            return {};

        auto& store = get_or_create_store<Texture>();
        auto& record = get_or_create_record(store, *entry);
        record.last_access = now;

        const TextureSettings& settings = parameters.settings;
        const bool should_reload = !record.asset;
        if (should_reload)
        {
            TBX_TRACE_INFO(
                "Loading asset: '{}' (id={}, type={}, settings={}/{}/{}/{}/{})",
                record.normalized_path,
                to_string(record.id),
                typeid(Texture).name(),
                static_cast<int>(settings.wrap),
                static_cast<int>(settings.filter),
                static_cast<int>(settings.format),
                static_cast<int>(settings.mipmaps),
                static_cast<int>(settings.compression));
            record.stream_state = AssetStreamState::LOADING;
            record.asset = load_texture(
                entry->resolved_path,
                settings.wrap,
                settings.filter,
                settings.format,
                settings.mipmaps,
                settings.compression);
            record.texture_settings = settings;
            record.has_texture_settings = true;
            record.pending_load = {};
            record.stream_state =
                record.asset ? AssetStreamState::LOADED : AssetStreamState::UNLOADED;
            if (!record.asset)
            {
                TBX_TRACE_WARNING(
                    "Failed to load asset: '{}' (id={}, type={})",
                    record.normalized_path,
                    to_string(record.id),
                    typeid(Texture).name());
            }
        }

        return record.asset;
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
