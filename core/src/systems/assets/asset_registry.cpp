#include "tbx/core/systems/assets/manager.h"
#include "tbx/core/utils/string_utils.h"
#include "tbx/core/interfaces/file_ops.h"
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace tbx
{
    namespace
    {
        Result make_failed_result(std::string report)
        {
            auto result = Result {};
            result.flag_failure(std::move(report));
            return result;
        }

        void append_report(Result& result, std::string report)
        {
            if (report.empty())
            {
                return;
            }

            auto merged = result.get_report();
            if (!merged.empty())
            {
                merged.append("; ");
            }
            merged.append(report);

            if (result.succeeded())
            {
                result.flag_success(std::move(merged));
                return;
            }

            result.flag_failure(std::move(merged));
        }

        void merge_result(Result& destination, const Result& source)
        {
            append_report(destination, source.get_report());
            if (!source.succeeded() && destination.succeeded())
            {
                destination.flag_failure(destination.get_report());
            }
        }

        bool path_contains_directory_token(
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

        bool is_non_asset_file(const std::filesystem::path& path)
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
    }

    bool AssetRegistry::should_track_asset_path(const std::filesystem::path& asset_path)
    {
        if (asset_path.empty())
            return false;
        if (path_contains_directory_token(asset_path, "generated"))
            return false;
        if (is_non_asset_file(asset_path))
            return false;
        return asset_path.extension() != ".meta";
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

    Result AssetRegistry::add_asset_directory(const std::filesystem::path& path)
    {
        if (path.empty())
        {
            return make_failed_result("Cannot add an empty asset directory path.");
        }

        auto resolved = _file_ops->resolve(path);
        if (resolved.empty())
        {
            return make_failed_result("Failed to resolve asset directory path.");
        }

        const bool is_duplicate = std::any_of(
            _asset_directories.begin(),
            _asset_directories.end(),
            [&resolved](const std::filesystem::path& existing)
            {
                return existing == resolved;
            });
        if (is_duplicate)
        {
            auto result = Result {};
            result.flag_success("Asset directory is already tracked.");
            return result;
        }

        _asset_directories.push_back(resolved);

        auto result = Result {};
        result.flag_success("Tracking asset directory.");

        const auto scan_result = scan_asset_directory(resolved);
        merge_result(result, scan_result);
        return result;
    }

    Result AssetRegistry::ensure_asset_id(const Handle& handle, Uuid* out_asset_id)
    {
        if (!out_asset_id)
        {
            return make_failed_result("Cannot ensure an asset id with a null output pointer.");
        }

        if (handle.get_name().empty())
        {
            if (!handle.get_id().is_valid())
            {
                return make_failed_result(
                    "Cannot ensure asset id: handle has no path and no valid id.");
            }

            auto* entry = find_entry_by_id(handle.get_id());
            *out_asset_id = entry ? entry->asset_id : handle.get_id();
            return {};
        }

        const AssetRegistryEntry* entry = nullptr;
        auto result = ensure_entry(handle, &entry);
        if (!result.succeeded())
        {
            return result;
        }

        if (!entry || !entry->asset_id.is_valid())
        {
            return make_failed_result(
                std::string("Resolved asset entry for '")
                    .append(handle.get_name())
                    .append("' does not contain a valid id."));
        }

        *out_asset_id = entry->asset_id;
        return result;
    }

    Result AssetRegistry::ensure_entry(const Handle& handle, const AssetRegistryEntry** out_entry)
    {
        if (!out_entry)
        {
            return make_failed_result("Cannot ensure an asset entry with a null output pointer.");
        }
        *out_entry = nullptr;

        auto result = Result {};

        if (!handle.get_name().empty())
        {
            auto& entry = get_or_create_path_entry(handle.get_name());
            if (entry.asset_id.is_valid())
            {
                *out_entry = &entry;
                return result;
            }

            auto asset_id = Uuid {};
            const auto resolve_result = resolve_or_repair_asset_id(entry, &asset_id);
            merge_result(result, resolve_result);
            if (!resolve_result.succeeded())
            {
                return result;
            }

            if (!asset_id.is_valid())
            {
                asset_id = make_runtime_asset_id(entry.normalized_path);
                append_report(
                    result,
                    std::string("Resolved an invalid asset id for '")
                        .append(entry.normalized_path)
                        .append("'. Using runtime id=")
                        .append(to_string(asset_id))
                        .append("."));
            }

            const auto assign_result = try_assign_asset_id(entry, asset_id);
            merge_result(result, assign_result);
            if (!assign_result.succeeded())
            {
                return result;
            }

            *out_entry = &entry;
            return result;
        }

        if (!handle.get_id().is_valid())
        {
            return make_failed_result("Cannot resolve an asset entry from an invalid handle id.");
        }

        auto* entry = find_entry_by_id(handle.get_id());
        if (!entry)
        {
            return make_failed_result(
                std::string("No registered asset entry exists for id=")
                    .append(to_string(handle.get_id()))
                    .append("."));
        }

        *out_entry = entry;
        return result;
    }

    const AssetRegistryEntry* AssetRegistry::find_entry(const Handle& handle) const
    {
        if (!handle.get_name().empty())
        {
            auto* entry = find_entry_by_path(handle.get_name());
            if (entry)
                return entry;
        }

        if (!handle.get_id().is_valid())
            return nullptr;

        return find_entry_by_id(handle.get_id());
    }

    std::vector<std::filesystem::path> AssetRegistry::get_asset_directories() const
    {
        return _asset_directories;
    }

    Result AssetRegistry::register_discovered_asset(
        const std::filesystem::path& asset_path,
        AssetRegistryEntry* out_entry)
    {
        if (!should_track_asset_path(asset_path))
        {
            return make_failed_result("Path is not a tracked asset.");
        }

        auto result = Result {};
        auto& entry = get_or_create_path_entry(asset_path);
        if (!entry.asset_id.is_valid())
        {
            const auto discovered_id = try_resolve_discovered_asset_id(entry);
            if (discovered_id.is_valid())
            {
                const auto assign_result = try_assign_asset_id(entry, discovered_id);
                merge_result(result, assign_result);
                if (!assign_result.succeeded())
                {
                    return result;
                }

                append_report(
                    result,
                    std::string("Indexed discovered asset id=")
                        .append(to_string(discovered_id))
                        .append(" for '")
                        .append(entry.normalized_path)
                        .append("'."));
            }
        }

        if (out_entry)
            *out_entry = entry;

        return result;
    }

    Result AssetRegistry::unregister_asset(
        const std::filesystem::path& asset_path,
        AssetRegistryEntry* out_removed_entry)
    {
        const auto normalized_path = normalize_path_string(asset_path);
        auto iterator = _entries_by_path.find(normalized_path);
        if (iterator == _entries_by_path.end())
        {
            return make_failed_result(
                std::string("Asset path is not registered: '")
                    .append(normalized_path)
                    .append("'."));
        }

        if (out_removed_entry)
            *out_removed_entry = iterator->second;

        if (iterator->second.asset_id.is_valid())
            _path_by_id.erase(iterator->second.asset_id);

        _entries_by_path.erase(iterator);
        return {};
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
        if (!handle.get_name().empty())
        {
            return resolve_asset_path(std::filesystem::path(handle.get_name()));
        }

        if (!handle.get_id().is_valid())
            return {};

        auto* entry = find_entry_by_id(handle.get_id());
        if (!entry)
            return {};

        return entry->resolved_path;
    }

    Result AssetRegistry::scan_asset_directory(const std::filesystem::path& root)
    {
        if (root.empty())
        {
            return make_failed_result("Cannot scan an empty asset directory root.");
        }

        auto result = Result {};
        auto entries = _file_ops->read_directory(root);
        auto asset_entries = std::vector<std::filesystem::path>();
        for (const auto& entry : entries)
        {
            if (_file_ops->get_type(entry) != FileType::FILE)
                continue;
            if (!should_track_asset_path(entry))
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
                const auto assign_result = try_assign_asset_id(registry_entry, discovered_id);
                merge_result(result, assign_result);
            }
        }

        return result;
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
            if (_handle_source(entry.resolved_path, handle) && handle.get_id().is_valid())
            {
                return handle.get_id();
            }
        }

        auto meta_path = entry.resolved_path;
        meta_path += ".meta";
        if (!_file_ops->exists(meta_path))
        {
            return {};
        }

        auto parsed_handle = _handle_serializer->read_from_disk(*_file_ops, entry.resolved_path);
        if (!parsed_handle || !parsed_handle->get_id().is_valid())
        {
            return {};
        }

        return parsed_handle->get_id();
    }

    Result AssetRegistry::resolve_or_repair_asset_id(
        const AssetRegistryEntry& entry,
        Uuid* out_asset_id) const
    {
        if (!out_asset_id)
        {
            return make_failed_result("Cannot resolve asset id with a null output pointer.");
        }

        auto result = Result {};

        if (_handle_source)
        {
            auto handle = Handle();
            if (_handle_source(entry.resolved_path, handle))
            {
                if (handle.get_id().is_valid())
                {
                    *out_asset_id = handle.get_id();
                    return result;
                }

                *out_asset_id = make_runtime_asset_id(entry.normalized_path);
                append_report(
                    result,
                    std::string("Handle source returned an invalid id for '")
                        .append(entry.normalized_path)
                        .append("'. Using runtime id=")
                        .append(to_string(*out_asset_id))
                        .append("."));
                return result;
            }
        }

        if (!_file_ops->exists(entry.resolved_path))
        {
            *out_asset_id = make_runtime_asset_id(entry.normalized_path);
            append_report(
                result,
                std::string("Requested asset '")
                    .append(entry.normalized_path)
                    .append("' was not found on disk. Using runtime id=")
                    .append(to_string(*out_asset_id))
                    .append("."));
            return result;
        }

        auto meta_path = entry.resolved_path;
        meta_path += ".meta";

        if (_file_ops->exists(meta_path))
        {
            auto parsed_handle =
                _handle_serializer->read_from_disk(*_file_ops, entry.resolved_path);
            if (parsed_handle && parsed_handle->get_id().is_valid())
            {
                *out_asset_id = parsed_handle->get_id();
                return result;
            }
        }

        auto generated_id = generate_unique_asset_id();
        if (!generated_id.is_valid())
        {
            return make_failed_result(
                std::string("Failed to generate a valid id for asset '")
                    .append(entry.normalized_path)
                    .append("'."));
        }

        const bool meta_exists = _file_ops->exists(meta_path);
        const auto write_result = write_meta_with_id(entry.resolved_path, generated_id);

        *out_asset_id = generated_id;

        if (!meta_exists)
        {
            append_report(
                result,
                std::string("Missing metadata for asset '")
                    .append(entry.normalized_path)
                    .append("'. Generated '")
                    .append(meta_path.generic_string())
                    .append("' with id=")
                    .append(to_string(generated_id))
                    .append("."));
        }
        else
        {
            append_report(
                result,
                std::string("Invalid metadata for asset '")
                    .append(entry.normalized_path)
                    .append("'. Rewrote '")
                    .append(meta_path.generic_string())
                    .append("' with id=")
                    .append(to_string(generated_id))
                    .append("."));
        }

        if (!write_result.succeeded())
        {
            append_report(
                result,
                std::string("Failed to write metadata sidecar '")
                    .append(meta_path.generic_string())
                    .append("' for asset '")
                    .append(entry.normalized_path)
                    .append("'. Using id=")
                    .append(to_string(generated_id))
                    .append(" for this session."));
        }

        return result;
    }

    Result AssetRegistry::try_assign_asset_id(AssetRegistryEntry& entry, Uuid asset_id)
    {
        if (!asset_id.is_valid())
        {
            return make_failed_result(
                std::string("Cannot assign an invalid id to asset '")
                    .append(entry.normalized_path)
                    .append("'."));
        }
        if (entry.asset_id == asset_id)
        {
            return {};
        }

        auto iterator = _path_by_id.find(asset_id);
        if (iterator != _path_by_id.end() && iterator->second != entry.normalized_path)
        {
            std::string existing_path = "<unknown>";
            auto existing_entry_iterator = _entries_by_path.find(iterator->second);
            if (existing_entry_iterator != _entries_by_path.end())
            {
                existing_path = existing_entry_iterator->second.normalized_path;
            }

            return make_failed_result(
                std::string("Duplicate asset id=")
                    .append(to_string(asset_id))
                    .append(" for '")
                    .append(entry.normalized_path)
                    .append("'; already used by '")
                    .append(existing_path)
                    .append("'."));
        }

        auto result = Result {};
        if (entry.asset_id.is_valid())
        {
            append_report(
                result,
                std::string("Reassigned asset id for '")
                    .append(entry.normalized_path)
                    .append("' from ")
                    .append(to_string(entry.asset_id))
                    .append(" to ")
                    .append(to_string(asset_id))
                    .append("."));
            _path_by_id.erase(entry.asset_id);
        }

        entry.asset_id = asset_id;
        _path_by_id[asset_id] = entry.normalized_path;
        return result;
    }

    Result AssetRegistry::write_meta_with_id(
        const std::filesystem::path& asset_path,
        Uuid asset_id) const
    {
        if (!_handle_serializer)
        {
            return Result(
                false,
                std::string("Asset handle serializer is unavailable for '")
                    .append(asset_path.generic_string())
                    .append("'."));
        }

        auto handle = Handle(asset_id);
        if (!_handle_serializer->try_write_to_disk(*_file_ops, asset_path, handle))
        {
            return Result(
                false,
                std::string("Failed to write metadata sidecar for '")
                    .append(asset_path.generic_string())
                    .append("'."));
        }

        return {};
    }
}
