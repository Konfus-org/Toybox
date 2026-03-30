#include "tbx/assets/asset_manager.h"
#include "tbx/common/string_utils.h"
#include "tbx/files/file_ops.h"
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace tbx
{
    static bool path_contains_directory_token(
        const std::filesystem::path& path,
        std::string_view directory_name_lowered)
    {
        if (directory_name_lowered.empty())
            return false;

        for (const auto& part : path)
        {
            if (tbx::to_lower(part.string()) == directory_name_lowered)
                return true;
        }

        return false;
    }

    static bool is_non_asset_file(const std::filesystem::path& path)
    {
        const auto lowered_name = tbx::to_lower(path.filename().string());
        if (lowered_name == "cmakelists.txt")
            return true;

        const auto lowered_extension = tbx::to_lower(path.extension().string());
        return lowered_extension == ".cmake" || lowered_extension == ".h"
               || lowered_extension == ".hh" || lowered_extension == ".hpp"
               || lowered_extension == ".c" || lowered_extension == ".cc"
               || lowered_extension == ".cpp" || lowered_extension == ".cxx"
               || lowered_extension == ".in";
    }

    static std::filesystem::path get_resources_directory()
    {
#if defined(TBX_RESOURCES)
        auto resources = std::filesystem::path(TBX_RESOURCES);
        return resources.lexically_normal();
#endif
        return {};
    }

    AssetRegistry::AssetRegistry(
        std::filesystem::path working_directory,
        HandleSource handle_source,
        std::unique_ptr<IAssetHandleSerializer> asset_handle_serializer,
        std::shared_ptr<IFileOps> file_ops)
        : _handle_source(std::move(handle_source))
        , _handle_serializer(std::move(asset_handle_serializer))
        , _file_ops(std::move(file_ops))
    {
        if (!_handle_serializer)
            _handle_serializer = std::make_unique<AssetHandleSerializer>();
        if (!_file_ops)
            _file_ops = std::make_shared<FileOperator>(std::move(working_directory));

        _working_directory = _file_ops->get_working_directory();
    }

    void AssetRegistry::add_asset_directory(const std::filesystem::path& path)
    {
        if (path.empty())
            return;

        auto resolved = _file_ops->resolve(path);
        if (resolved.empty())
            return;

        const bool is_duplicate = std::any_of(
            _asset_directories.begin(),
            _asset_directories.end(),
            [&resolved](const std::filesystem::path& existing)
            {
                return existing == resolved;
            });
        if (is_duplicate)
            return;

        _asset_directories.push_back(resolved);
        scan_asset_directory(resolved);
    }

    Uuid AssetRegistry::ensure_asset_id(const Handle& handle)
    {
        auto* entry = ensure_entry(handle);
        if (!entry)
        {
            if (handle.name.empty())
                return handle.id;
            return {};
        }

        return entry->asset_id;
    }

    const AssetRegistryEntry* AssetRegistry::ensure_entry(const Handle& handle)
    {
        if (!handle.name.empty())
        {
            auto& entry = get_or_create_path_entry(handle.name);
            if (entry.asset_id.is_valid())
            {
                return &entry;
            }

            auto asset_id = resolve_or_repair_asset_id(entry);
            if (!asset_id.is_valid())
            {
                asset_id = make_runtime_asset_id(entry.normalized_path);
            }
            if (!try_assign_asset_id(entry, asset_id))
            {
                return nullptr;
            }

            return &entry;
        }

        if (!handle.id.is_valid())
            return nullptr;

        return find_entry_by_id(handle.id);
    }

    const AssetRegistryEntry* AssetRegistry::find_entry(const Handle& handle) const
    {
        if (!handle.name.empty())
        {
            auto* entry = find_entry_by_path(handle.name);
            if (entry)
                return entry;
        }

        if (!handle.id.is_valid())
            return nullptr;

        return find_entry_by_id(handle.id);
    }

    std::vector<std::filesystem::path> AssetRegistry::get_asset_directories() const
    {
        return _asset_directories;
    }

    std::filesystem::path AssetRegistry::resolve_asset_path(
        const std::filesystem::path& asset_path) const
    {
        if (asset_path.empty())
            return asset_path;
        if (asset_path.is_absolute())
            return asset_path;

        for (const auto& root : _asset_directories)
        {
            if (root.empty())
                continue;

            auto candidate = _file_ops->resolve(root / asset_path);
            if (_file_ops->exists(candidate))
                return candidate;
        }

        return _file_ops->resolve(asset_path);
    }

    std::filesystem::path AssetRegistry::resolve_asset_path(const Handle& handle) const
    {
        if (!handle.name.empty())
        {
            return resolve_asset_path(std::filesystem::path(handle.name));
        }

        if (!handle.id.is_valid())
            return {};

        auto* entry = find_entry_by_id(handle.id);
        if (!entry)
            return {};

        return entry->resolved_path;
    }

    AssetRegistryEntry* AssetRegistry::find_entry_by_id(Uuid asset_id)
    {
        return const_cast<AssetRegistryEntry*>(
            static_cast<const AssetRegistry*>(this)->find_entry_by_id(asset_id));
    }

    const AssetRegistryEntry* AssetRegistry::find_entry_by_id(Uuid asset_id) const
    {
        if (!asset_id)
            return nullptr;

        auto iterator = _path_by_id.find(asset_id);
        if (iterator == _path_by_id.end())
            return nullptr;

        auto entry_iterator = _entries_by_path.find(iterator->second);
        if (entry_iterator == _entries_by_path.end())
            return nullptr;

        return &entry_iterator->second;
    }

    AssetRegistryEntry* AssetRegistry::find_entry_by_path(const std::filesystem::path& asset_path)
    {
        return const_cast<AssetRegistryEntry*>(
            static_cast<const AssetRegistry*>(this)->find_entry_by_path(asset_path));
    }

    const AssetRegistryEntry* AssetRegistry::find_entry_by_path(
        const std::filesystem::path& asset_path) const
    {
        auto normalized = normalize_path_string(asset_path);
        auto iterator = _entries_by_path.find(normalized);
        if (iterator == _entries_by_path.end())
            return nullptr;

        return &iterator->second;
    }

    Uuid AssetRegistry::make_runtime_asset_id(const std::string& normalized_path)
    {
        const auto hasher = std::hash<std::string>();
        auto hashed = static_cast<uint32>(hasher(normalized_path));
        if (hashed == 0U)
        {
            hashed = 1U;
        }
        return Uuid(hashed);
    }

    Uuid AssetRegistry::generate_unique_asset_id() const
    {
        auto generated = Uuid::generate();
        while (!generated.is_valid() || _path_by_id.contains(generated))
            generated = Uuid::generate();
        return generated;
    }

    AssetRegistryEntry& AssetRegistry::get_or_create_path_entry(
        const std::filesystem::path& asset_path)
    {
        auto resolved_path = resolve_asset_path(asset_path);
        auto normalized_path = resolved_path.lexically_normal().generic_string();
        auto iterator = _entries_by_path.find(normalized_path);
        if (iterator != _entries_by_path.end())
        {
            return iterator->second;
        }

        AssetRegistryEntry entry = {};
        entry.resolved_path = std::move(resolved_path);
        entry.normalized_path = normalized_path;
        auto [inserted, was_inserted] = _entries_by_path.emplace(normalized_path, std::move(entry));
        static_cast<void>(was_inserted);
        return inserted->second;
    }

    std::string AssetRegistry::normalize_path_string(const std::filesystem::path& asset_path) const
    {
        return resolve_asset_path(asset_path).lexically_normal().generic_string();
    }

    Uuid AssetRegistry::try_resolve_discovered_asset_id(const AssetRegistryEntry& entry) const
    {
        if (_handle_source)
        {
            auto handle = Handle();
            if (_handle_source(entry.resolved_path, handle) && handle.id.is_valid())
            {
                return handle.id;
            }
        }

        auto meta_path = entry.resolved_path;
        meta_path += ".meta";
        if (!_file_ops->exists(meta_path))
        {
            return {};
        }

        auto parsed_handle = _handle_serializer->read_from_disk(*_file_ops, entry.resolved_path);
        if (!parsed_handle || !parsed_handle->id.is_valid())
        {
            return {};
        }

        return parsed_handle->id;
    }

    Uuid AssetRegistry::resolve_or_repair_asset_id(const AssetRegistryEntry& entry)
    {
        if (_handle_source)
        {
            auto handle = Handle();
            if (_handle_source(entry.resolved_path, handle))
            {
                if (handle.id.is_valid())
                {
                    return handle.id;
                }
                return make_runtime_asset_id(entry.normalized_path);
            }
        }

        if (!_file_ops->exists(entry.resolved_path))
        {
            TBX_TRACE_WARNING(
                "AssetManager: requested asset '{}' was not found on disk. Using runtime id only.",
                entry.normalized_path);
            return make_runtime_asset_id(entry.normalized_path);
        }

        auto meta_path = entry.resolved_path;
        meta_path += ".meta";

        if (_file_ops->exists(meta_path))
        {
            auto parsed_handle =
                _handle_serializer->read_from_disk(*_file_ops, entry.resolved_path);
            if (parsed_handle && parsed_handle->id.is_valid())
            {
                return parsed_handle->id;
            }
        }

        auto generated_id = generate_unique_asset_id();
        const bool meta_exists = _file_ops->exists(meta_path);
        const bool write_success = write_meta_with_id(entry.resolved_path, generated_id);
        if (!meta_exists)
        {
            TBX_TRACE_WARNING(
                "AssetManager: missing metadata for asset '{}'. Generated '{}' with id={}.",
                entry.normalized_path,
                meta_path.generic_string(),
                to_string(generated_id));
        }
        else
        {
            TBX_TRACE_WARNING(
                "AssetManager: invalid metadata for asset '{}'. Rewrote '{}' with id={}.",
                entry.normalized_path,
                meta_path.generic_string(),
                to_string(generated_id));
        }

        if (!write_success)
        {
            TBX_TRACE_WARNING(
                "AssetManager: failed to write metadata sidecar '{}' for asset '{}'. Using id={} "
                "for this session.",
                meta_path.generic_string(),
                entry.normalized_path,
                to_string(generated_id));
        }

        return generated_id;
    }

    bool AssetRegistry::try_assign_asset_id(AssetRegistryEntry& entry, Uuid asset_id)
    {
        if (!asset_id.is_valid())
        {
            return false;
        }
        if (entry.asset_id == asset_id)
        {
            return true;
        }

        auto iterator = _path_by_id.find(asset_id);
        if (iterator != _path_by_id.end() && iterator->second != entry.normalized_path)
        {
            std::string existing_path = "<unknown>";
            auto existing_entry_iterator = _entries_by_path.find(iterator->second);
            if (existing_entry_iterator != _entries_by_path.end())
                existing_path = existing_entry_iterator->second.normalized_path;

            TBX_TRACE_ERROR(
                "AssetManager: skipping asset '{}' due to duplicate id={} already used by '{}'.",
                entry.normalized_path,
                to_string(asset_id),
                existing_path);
            return false;
        }

        if (entry.asset_id.is_valid())
        {
            _path_by_id.erase(entry.asset_id);
        }
        entry.asset_id = asset_id;
        _path_by_id[asset_id] = entry.normalized_path;
        return true;
    }

    bool AssetRegistry::write_meta_with_id(const std::filesystem::path& asset_path, Uuid asset_id)
        const
    {
        auto handle = Handle(asset_id);
        return _handle_serializer
               && _handle_serializer->try_write_to_disk(*_file_ops, asset_path, handle);
    }

    void AssetRegistry::scan_asset_directory(const std::filesystem::path& root)
    {
        if (root.empty())
            return;

        auto entries = _file_ops->read_directory(root);
        auto asset_entries = std::vector<std::filesystem::path>();
        for (const auto& entry : entries)
        {
            if (path_contains_directory_token(entry, "generated"))
                continue;
            if (is_non_asset_file(entry))
                continue;
            if (_file_ops->get_type(entry) != FileType::FILE)
                continue;
            if (entry.extension() == ".meta")
                continue;

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
            auto& registry_entry = get_or_create_path_entry(entry);
            if (registry_entry.asset_id.is_valid())
            {
                continue;
            }

            auto discovered_id = try_resolve_discovered_asset_id(registry_entry);
            if (discovered_id.is_valid())
            {
                static_cast<void>(try_assign_asset_id(registry_entry, discovered_id));
            }
        }
    }
}

namespace tbx
{
    AssetManager::AssetManager(
        std::filesystem::path working_directory,
        std::vector<std::filesystem::path> asset_directories,
        HandleSource handle_source,
        bool include_default_resources,
        std::unique_ptr<IAssetHandleSerializer> asset_handle_serializer,
        std::shared_ptr<IFileOps> file_ops)
        : _registry(
              std::make_unique<AssetRegistry>(
                  std::move(working_directory),
                  std::move(handle_source),
                  std::move(asset_handle_serializer),
                  std::move(file_ops)))
    {
        for (const auto& directory : asset_directories)
            add_asset_directory(directory);

        if (include_default_resources)
        {
#if defined(TBX_RELEASE)
            // Release flattens all assets into one resources dir
            add_asset_directory("resources");
#else
            // Debug keeps asset roots separate for debugging and hot reloading.
            auto resources = get_resources_directory();
            if (!resources.empty())
                add_asset_directory(resources);
#endif
        }
    }

    AssetManager::~AssetManager() = default;

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

    Uuid AssetManager::ensure_asset_id(const Handle& handle)
    {
        std::lock_guard lock(_mutex);
        return _registry->ensure_asset_id(handle);
    }

    Uuid AssetManager::resolve_asset_id(const Handle& handle)
    {
        return ensure_asset_id(handle);
    }

    std::filesystem::path AssetManager::resolve_asset_path(
        const std::filesystem::path& asset_path) const
    {
        std::lock_guard lock(_mutex);
        return _registry->resolve_asset_path(asset_path);
    }

    std::filesystem::path AssetManager::resolve_asset_path(const Handle& handle) const
    {
        std::lock_guard lock(_mutex);
        return _registry->resolve_asset_path(handle);
    }

    void AssetManager::set_pinned(const Handle& handle, bool is_pinned)
    {
        std::lock_guard lock(_mutex);
        auto* entry = _registry->find_entry(handle);
        if (!entry || !entry->asset_id.is_valid())
            return;

        for (auto& store : _stores)
            store.second->set_pinned(entry->asset_id, is_pinned);
    }

    void AssetManager::add_asset_directory(const std::filesystem::path& path)
    {
        if (path.empty())
            return;

        std::lock_guard lock(_mutex);
        _registry->add_asset_directory(path);
    }

    std::vector<std::filesystem::path> AssetManager::get_asset_directories() const
    {
        std::lock_guard lock(_mutex);
        return _registry->get_asset_directories();
    }
}
