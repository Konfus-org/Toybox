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
#include <string>
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
        Result result = {};
        std::optional<std::reference_wrapper<const AssetRegistryEntry>> entry = std::nullopt;
    };

    struct AssetRegistryMutationResult
    {
        Result result = {};
        std::optional<AssetRegistryEntry> entry = std::nullopt;
    };

    class TBX_API AssetRegistry final
    {
      public:
        AssetRegistry(
            std::filesystem::path working_directory,
            HandleSource handle_source,
            std::shared_ptr<IFileOps> file_ops);

      public:
        Result add_asset_directory(const std::filesystem::path& path);
        Result remove_asset_directory(const std::filesystem::path& path);
        Result ensure_asset_id(const Handle& handle, Uuid& out_asset_id);
        AssetRegistryEntryResult ensure_entry(const Handle& handle);
        std::optional<std::reference_wrapper<const AssetRegistryEntry>> find_entry(
            const Handle& handle) const;
        std::vector<std::filesystem::path> get_asset_directories() const;
        AssetRegistryMutationResult register_discovered_asset(
            const std::filesystem::path& asset_path);
        AssetRegistryMutationResult unregister_asset(const std::filesystem::path& asset_path);
        std::filesystem::path resolve_asset_path(const std::filesystem::path& asset_path) const;
        std::filesystem::path resolve_asset_path(const Handle& handle) const;
        Result scan_asset_directory(const std::filesystem::path& root);
        static bool should_track_asset_path(const std::filesystem::path& asset_path);

      private:
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
        std::string normalize_path_string(const std::filesystem::path& asset_path) const;
        Uuid try_resolve_discovered_asset_id(const AssetRegistryEntry& entry) const;
        Result resolve_or_repair_asset_id(const AssetRegistryEntry& entry, Uuid& out_asset_id)
            const;
        Result try_assign_asset_id(AssetRegistryEntry& entry, Uuid asset_id);

      private:
        std::filesystem::path _working_directory = {};
        HandleSource _handle_source = {};
        std::shared_ptr<IFileOps> _file_ops =
            nullptr; // TODO: Use weak pointer for file ops and other services to avoid holding refs
                     // when we shouldn't, if something should be fully owned then it should be a
                     // unique pointer
        std::vector<std::filesystem::path> _asset_directories = {};
        std::unordered_map<std::string, AssetRegistryEntry> _entries_by_path = {};
        std::unordered_map<Uuid, std::string> _path_by_id = {};
    };

}
