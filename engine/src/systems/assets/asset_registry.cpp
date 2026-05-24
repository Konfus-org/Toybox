#include "tbx/interfaces/file_ops.h"
#include "systems/assets/internal/asset_registry_internal.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/files/json.h"
#include "tbx/utils/string_utils.h"
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
namespace tbx
{
    bool AssetRegistry::should_track_asset_path(const std::filesystem::path& asset_path)
    {
        if (asset_path.empty())
            return false;
        if (internal::path_contains_directory_token(asset_path, "generated"))
            return false;
        if (internal::is_non_asset_file(asset_path))
            return false;
        return asset_path.extension() != ".meta";
    }

    AssetRegistry::AssetRegistry(
        std::filesystem::path working_directory,
        HandleSource handle_source,
        std::shared_ptr<IFileOps> file_ops)
        : _handle_source(std::move(handle_source))
        , _file_ops(std::move(file_ops))
    {
        if (!_file_ops)
            _file_ops = std::make_shared<FileOperator>(std::move(working_directory));

        _working_directory = _file_ops->get_working_directory();
    }

    Result AssetRegistry::add_asset_directory(const std::filesystem::path& path)
    {
        if (path.empty())
        {
            return internal::make_failed_result("Cannot add an empty asset directory path.");
        }

        auto resolved = _file_ops->resolve(path);
        if (resolved.empty())
        {
            return internal::make_failed_result("Failed to resolve asset directory path.");
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
        internal::merge_result(result, scan_result);
        return result;
    }

    Result AssetRegistry::ensure_asset_id(const Handle& handle, Uuid& out_asset_id)
    {
        if (handle.get_name().empty())
        {
            if (!handle.get_id().is_valid())
            {
                return internal::make_failed_result(
                    "Cannot ensure asset id: handle has no path and no valid id.");
            }

            auto entry = find_entry_by_id(handle.get_id());
            out_asset_id = entry.has_value() ? entry->get().asset_id : handle.get_id();
            return {};
        }

        auto ensure_result = ensure_entry(handle);
        if (!ensure_result.result.succeeded())
        {
            return ensure_result.result;
        }

        if (!ensure_result.entry.has_value() || !ensure_result.entry->get().asset_id.is_valid())
        {
            return internal::make_failed_result(
                std::string("Resolved asset entry for '")
                    .append(handle.get_name())
                    .append("' does not contain a valid id."));
        }

        out_asset_id = ensure_result.entry->get().asset_id;
        return ensure_result.result;
    }

    AssetRegistryEntryResult AssetRegistry::ensure_entry(const Handle& handle)
    {
        auto result = AssetRegistryEntryResult {};

        if (!handle.get_name().empty())
        {
            auto& entry = get_or_create_path_entry(handle.get_name());
            if (entry.asset_id.is_valid())
            {
                result.entry = std::cref(entry);
                return result;
            }

            auto asset_id = Uuid {};
            const auto resolve_result = resolve_or_repair_asset_id(entry, asset_id);
            internal::merge_result(result.result, resolve_result);
            if (!resolve_result.succeeded())
            {
                return result;
            }

            if (!asset_id.is_valid())
            {
                asset_id = make_runtime_asset_id(entry.normalized_path);
                internal::append_report(
                    result.result,
                    std::string("Resolved an invalid asset id for '")
                        .append(entry.normalized_path)
                        .append("'. Using runtime id=")
                        .append(to_string(asset_id))
                        .append("."));
            }

            const auto assign_result = try_assign_asset_id(entry, asset_id);
            internal::merge_result(result.result, assign_result);
            if (!assign_result.succeeded())
            {
                return result;
            }

            result.entry = std::cref(entry);
            return result;
        }

        if (!handle.get_id().is_valid())
        {
            result.result = internal::make_failed_result(
                "Cannot resolve an asset entry from an invalid handle id.");
            return result;
        }

        auto entry = find_entry_by_id(handle.get_id());
        if (!entry.has_value())
        {
            result.result = internal::make_failed_result(
                std::string("No registered asset entry exists for id=")
                    .append(to_string(handle.get_id()))
                    .append("."));
            return result;
        }

        result.entry = std::cref(entry->get());
        return result;
    }

    std::optional<std::reference_wrapper<const AssetRegistryEntry>> AssetRegistry::find_entry(
        const Handle& handle) const
    {
        if (!handle.get_name().empty())
        {
            auto entry = find_entry_by_path(handle.get_name());
            if (entry.has_value())
                return entry;
        }

        if (!handle.get_id().is_valid())
            return std::nullopt;

        return find_entry_by_id(handle.get_id());
    }

    std::vector<std::filesystem::path> AssetRegistry::get_asset_directories() const
    {
        return _asset_directories;
    }

    AssetRegistryMutationResult AssetRegistry::register_discovered_asset(
        const std::filesystem::path& asset_path)
    {
        auto result = AssetRegistryMutationResult {};
        if (!should_track_asset_path(asset_path))
        {
            result.result = internal::make_failed_result("Path is not a tracked asset.");
            return result;
        }

        auto& entry = get_or_create_path_entry(asset_path);
        if (!entry.asset_id.is_valid())
        {
            const auto discovered_id = try_resolve_discovered_asset_id(entry);
            if (discovered_id.is_valid())
            {
                const auto assign_result = try_assign_asset_id(entry, discovered_id);
                internal::merge_result(result.result, assign_result);
                if (!assign_result.succeeded())
                {
                    return result;
                }

                internal::append_report(
                    result.result,
                    std::string("Indexed discovered asset id=")
                        .append(to_string(discovered_id))
                        .append(" for '")
                        .append(entry.normalized_path)
                        .append("'."));
            }
        }

        result.entry = entry;
        return result;
    }

    AssetRegistryMutationResult AssetRegistry::unregister_asset(
        const std::filesystem::path& asset_path)
    {
        auto result = AssetRegistryMutationResult {};
        const auto normalized_path = normalize_path_string(asset_path);
        auto iterator = _entries_by_path.find(normalized_path);
        if (iterator == _entries_by_path.end())
        {
            result.result = internal::make_failed_result(
                std::string("Asset path is not registered: '")
                    .append(normalized_path)
                    .append("'."));
            return result;
        }

        result.entry = iterator->second;

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

        auto entry = find_entry_by_id(handle.get_id());
        if (!entry.has_value())
            return {};

        return entry->get().resolved_path;
    }

    Result AssetRegistry::scan_asset_directory(const std::filesystem::path& root)
    {
        if (root.empty())
        {
            return internal::make_failed_result("Cannot scan an empty asset directory root.");
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
                internal::merge_result(result, assign_result);
            }
        }

        return result;
    }

    std::optional<std::reference_wrapper<AssetRegistryEntry>> AssetRegistry::find_entry_by_id(
        Uuid asset_id)
    {
        const auto& self = static_cast<const AssetRegistry&>(*this);
        auto entry = self.find_entry_by_id(asset_id);
        if (!entry.has_value())
            return std::nullopt;

        return std::ref(const_cast<AssetRegistryEntry&>(entry->get()));
    }

    std::optional<std::reference_wrapper<const AssetRegistryEntry>> AssetRegistry::find_entry_by_id(
        Uuid asset_id) const
    {
        if (!asset_id)
            return std::nullopt;

        auto iterator = _path_by_id.find(asset_id);
        if (iterator == _path_by_id.end())
            return std::nullopt;

        auto entry_iterator = _entries_by_path.find(iterator->second);
        if (entry_iterator == _entries_by_path.end())
            return std::nullopt;

        return std::cref(entry_iterator->second);
    }

    std::optional<std::reference_wrapper<AssetRegistryEntry>> AssetRegistry::find_entry_by_path(
        const std::filesystem::path& asset_path)
    {
        const auto& self = static_cast<const AssetRegistry&>(*this);
        auto entry = self.find_entry_by_path(asset_path);
        if (!entry.has_value())
            return std::nullopt;

        return std::ref(const_cast<AssetRegistryEntry&>(entry->get()));
    }

    std::optional<std::reference_wrapper<const AssetRegistryEntry>> AssetRegistry::
        find_entry_by_path(const std::filesystem::path& asset_path) const
    {
        auto normalized = normalize_path_string(asset_path);
        auto iterator = _entries_by_path.find(normalized);
        if (iterator == _entries_by_path.end())
            return std::nullopt;

        return std::cref(iterator->second);
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

        auto meta_path = internal::make_meta_path(entry.resolved_path);
        if (!_file_ops->exists(meta_path))
        {
            return {};
        }

        auto parsed_handle = internal::try_read_handle_from_meta(*_file_ops, entry.resolved_path);
        if (!parsed_handle || !parsed_handle->get_id().is_valid())
        {
            return {};
        }

        return parsed_handle->get_id();
    }

    Result AssetRegistry::resolve_or_repair_asset_id(
        const AssetRegistryEntry& entry,
        Uuid& out_asset_id) const
    {
        auto result = Result {};

        if (_handle_source)
        {
            auto handle = Handle();
            if (_handle_source(entry.resolved_path, handle))
            {
                if (handle.get_id().is_valid())
                {
                    out_asset_id = handle.get_id();
                    return result;
                }

                out_asset_id = make_runtime_asset_id(entry.normalized_path);
                internal::append_report(
                    result,
                    std::string("Handle source returned an invalid id for '")
                        .append(entry.normalized_path)
                        .append("'. Using runtime id=")
                        .append(to_string(out_asset_id))
                        .append("."));
                return result;
            }
        }

        if (!_file_ops->exists(entry.resolved_path))
        {
            out_asset_id = make_runtime_asset_id(entry.normalized_path);
            internal::append_report(
                result,
                std::string("Requested asset '")
                    .append(entry.normalized_path)
                    .append("' was not found on disk. Using runtime id=")
                    .append(to_string(out_asset_id))
                    .append("."));
            return result;
        }

        auto meta_path = internal::make_meta_path(entry.resolved_path);

        if (_file_ops->exists(meta_path))
        {
            auto parsed_handle =
                internal::try_read_handle_from_meta(*_file_ops, entry.resolved_path);
            if (parsed_handle && parsed_handle->get_id().is_valid())
            {
                out_asset_id = parsed_handle->get_id();
                return result;
            }
        }

        auto generated_id = generate_unique_asset_id();
        if (!generated_id.is_valid())
        {
            return internal::make_failed_result(
                std::string("Failed to generate a valid id for asset '")
                    .append(entry.normalized_path)
                    .append("'."));
        }

        out_asset_id = generated_id;

        if (!_file_ops->exists(meta_path))
        {
            TBX_TRACE_WARNING(
                "Missing metadata sidecar for asset '{}'. Generated in-memory id={}.",
                entry.normalized_path,
                to_string(generated_id));
            internal::append_report(
                result,
                std::string("Missing metadata for asset '")
                    .append(entry.normalized_path)
                    .append("'. Generated in-memory id=")
                    .append(to_string(generated_id))
                    .append("."));
        }
        else
        {
            TBX_TRACE_WARNING(
                "Invalid metadata sidecar for asset '{}'. Generated in-memory id={}.",
                entry.normalized_path,
                to_string(generated_id));
            internal::append_report(
                result,
                std::string("Invalid metadata for asset '")
                    .append(entry.normalized_path)
                    .append("'. Generated in-memory id=")
                    .append(to_string(generated_id))
                    .append("."));
        }

        return result;
    }

    Result AssetRegistry::try_assign_asset_id(AssetRegistryEntry& entry, Uuid asset_id)
    {
        if (!asset_id.is_valid())
        {
            return internal::make_failed_result(
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

            return internal::make_failed_result(
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
            internal::append_report(
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
}
