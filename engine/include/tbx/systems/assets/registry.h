#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/utils/result.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tbx
{
    using HandleSource = std::function<bool(const std::filesystem::path&, Handle& out_handle)>;

    struct AssetRegistryEntry
    {
        std::filesystem::path resolved_path = {};
        std::string normalized_path = {};
        Uuid asset_id = {};
    };

    struct AssetRegistryEntryResult
    {
        Result result = Result();
        std::optional<std::reference_wrapper<const AssetRegistryEntry>> entry = std::nullopt;
    };

    struct AssetRegistryMutationResult
    {
        Result result = Result();
        std::optional<AssetRegistryEntry> entry = std::nullopt;
    };

    struct AssetRegistryDirectoryRemovalResult
    {
        Result result = Result();
        std::vector<AssetRegistryEntry> entries = {};
    };

    class TBX_API AssetRegistry final
    {
      public:
        AssetRegistry(
            std::filesystem::path working_directory,
            HandleSource handle_source,
            std::weak_ptr<IFileOps> file_ops);

      public:
        Result add_asset_directory(const std::filesystem::path& path);
        AssetRegistryDirectoryRemovalResult remove_asset_directory(
            const std::filesystem::path& path);
        Result ensure_asset_id(const Handle& handle, Uuid& out_asset_id);
        AssetRegistryEntryResult ensure_entry(const Handle& handle);
        std::optional<std::reference_wrapper<const AssetRegistryEntry>> find_entry(
            const Handle& handle) const;
        // Finds a registered asset by its bare name (filename stem, case-insensitive), trying the given
        // file extensions in priority order so an earlier extension wins over a later same-named one.
        // Used to auto-bind a model's material slot to an asset of the same name. Null when nothing matches.
        std::optional<std::reference_wrapper<const AssetRegistryEntry>> find_entry_by_name(
            std::string_view name, std::span<const std::string_view> extensions) const;
        std::vector<std::filesystem::path> get_asset_directories() const;
        std::vector<AssetRegistryEntry> get_entries() const;
        AssetRegistryMutationResult register_discovered_asset(
            const std::filesystem::path& asset_path);
        AssetRegistryMutationResult unregister_asset(const std::filesystem::path& asset_path);
        // Forgets an asset addressed by handle rather than path. Unlike the path overload this never routes
        // through resolve_asset_path (whose root-resolution needs the file to still exist), so it works after
        // the file has already been deleted: it matches by the stable id when valid, else by the handle's
        // name used directly as the stored key (verbatim, then lexically normalized).
        AssetRegistryMutationResult unregister_asset(const Handle& handle);
        std::filesystem::path resolve_asset_path(const std::filesystem::path& asset_path) const;
        std::filesystem::path resolve_asset_path(const Handle& handle) const;
        Result scan_asset_directory(const std::filesystem::path& root);
        static bool should_track_asset_path(const std::filesystem::path& asset_path);

      private:
        // Removes the entry at `iterator` from both indexes (_entries_by_path and _path_by_id),
        // returning the removed entry. The shared erase tail of the unregister_asset overloads.
        AssetRegistryEntry erase_entry(
            std::unordered_map<std::string, AssetRegistryEntry>::iterator iterator);
        std::optional<std::reference_wrapper<AssetRegistryEntry>> find_entry_by_id(Uuid asset_id);
        std::optional<std::reference_wrapper<const AssetRegistryEntry>> find_entry_by_id(
            Uuid asset_id) const;
        std::optional<std::reference_wrapper<AssetRegistryEntry>> find_entry_by_path(
            const std::filesystem::path& asset_path);
        std::optional<std::reference_wrapper<const AssetRegistryEntry>> find_entry_by_path(
            const std::filesystem::path& asset_path) const;
        static Uuid make_runtime_asset_id(const std::string& normalized_path);
        Uuid generate_unique_asset_id() const;
        AssetRegistryEntry& get_or_create_path_entry(const std::filesystem::path& asset_path);
        std::shared_ptr<IFileOps> lock_file_ops() const;
        std::string normalize_path_string(const std::filesystem::path& asset_path) const;
        Uuid try_resolve_discovered_asset_id(const AssetRegistryEntry& entry) const;
        Result resolve_or_repair_asset_id(const AssetRegistryEntry& entry, Uuid& out_asset_id)
            const;
        Result try_assign_asset_id(AssetRegistryEntry& entry, Uuid asset_id);

      private:
        std::filesystem::path _working_directory = {};
        HandleSource _handle_source = {};
        std::shared_ptr<IFileOps> _owned_file_ops = nullptr;
        std::weak_ptr<IFileOps> _file_ops = {};
        std::vector<std::filesystem::path> _asset_directories = {};
        std::unordered_map<std::string, AssetRegistryEntry> _entries_by_path = {};
        std::unordered_map<Uuid, std::string> _path_by_id = {};
    };

}

