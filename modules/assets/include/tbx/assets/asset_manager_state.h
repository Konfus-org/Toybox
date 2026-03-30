#pragma once
#include "tbx/assets/asset_handle_serializer.h"
#include "tbx/assets/asset_loaders.h"
#include "tbx/common/handle.h"
#include "tbx/common/result.h"
#include "tbx/common/typedefs.h"
#include "tbx/common/uuid.h"
#include "tbx/debugging/macros.h"
#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tbx
{
    struct AssetRegistryEntry
    {
        std::filesystem::path resolved_path = {};
        std::string normalized_path = {};
        Uuid asset_id = {};
    };

    class TBX_API AssetRegistry final
    {
      public:
        AssetRegistry(
            std::filesystem::path working_directory,
            HandleSource handle_source,
            std::unique_ptr<IAssetHandleSerializer> asset_handle_serializer,
            std::shared_ptr<IFileOps> file_ops);

      public:
        void add_asset_directory(const std::filesystem::path& path);
        Uuid ensure_asset_id(const Handle& handle);
        const AssetRegistryEntry* ensure_entry(const Handle& handle);
        const AssetRegistryEntry* find_entry(const Handle& handle) const;
        std::vector<std::filesystem::path> get_asset_directories() const;
        std::filesystem::path resolve_asset_path(const std::filesystem::path& asset_path) const;
        std::filesystem::path resolve_asset_path(const Handle& handle) const;

      private:
        AssetRegistryEntry* find_entry_by_id(Uuid asset_id);
        const AssetRegistryEntry* find_entry_by_id(Uuid asset_id) const;
        AssetRegistryEntry* find_entry_by_path(const std::filesystem::path& asset_path);
        const AssetRegistryEntry* find_entry_by_path(const std::filesystem::path& asset_path)
            const;
        static Uuid make_runtime_asset_id(const std::string& normalized_path);
        Uuid generate_unique_asset_id() const;
        AssetRegistryEntry& get_or_create_path_entry(const std::filesystem::path& asset_path);
        std::string normalize_path_string(const std::filesystem::path& asset_path) const;
        Uuid try_resolve_discovered_asset_id(const AssetRegistryEntry& entry) const;
        Uuid resolve_or_repair_asset_id(const AssetRegistryEntry& entry);
        bool try_assign_asset_id(AssetRegistryEntry& entry, Uuid asset_id);
        bool write_meta_with_id(const std::filesystem::path& asset_path, Uuid asset_id) const;
        void scan_asset_directory(const std::filesystem::path& root);

      private:
        std::filesystem::path _working_directory = {};
        HandleSource _handle_source = {};
        std::unique_ptr<IAssetHandleSerializer> _handle_serializer = nullptr;
        std::shared_ptr<IFileOps> _file_ops = nullptr;
        std::vector<std::filesystem::path> _asset_directories = {};
        std::unordered_map<std::string, AssetRegistryEntry> _entries_by_path = {};
        std::unordered_map<Uuid, std::string> _path_by_id = {};
    };

    struct IAssetStore
    {
        virtual ~IAssetStore() = default;
        virtual void unload_unreferenced() = 0;
        virtual void set_pinned(Uuid asset_id, bool is_pinned) = 0;
    };

    template <typename TAsset>
    struct AssetRecord
    {
        std::shared_ptr<TAsset> asset = {};
        std::string normalized_path = {};
        bool is_pinned = false;
        AssetStreamState stream_state = AssetStreamState::UNLOADED;
        std::chrono::steady_clock::time_point last_access = {};
        Uuid asset_id = {};
        std::shared_future<Result> pending_load = {};
        AssetLoadParameters<TAsset> load_parameters = {};
        bool has_load_parameters = false;
    };

    template <typename TAsset>
    struct AssetStore final : IAssetStore
    {
        std::unordered_map<Uuid, AssetRecord<TAsset>> records = {};

        void unload_unreferenced() override
        {
            for (auto& entry : records)
            {
                auto& record = entry.second;
                if (!record.is_pinned && record.asset && record.asset.use_count() <= 1)
                {
                    TBX_TRACE_INFO(
                        "Unloaded asset: '{}' (id={}, type={})",
                        record.normalized_path,
                        to_string(record.asset_id),
                        typeid(TAsset).name());
                    record.asset.reset();
                    record.stream_state = AssetStreamState::UNLOADED;
                }
            }
        }

        void set_pinned(Uuid asset_id, bool is_pinned) override
        {
            auto iterator = records.find(asset_id);
            if (iterator == records.end())
                return;

            iterator->second.is_pinned = is_pinned;
        }
    };

    template <typename TAsset>
    bool asset_load_parameters_match(
        const AssetRecord<TAsset>& record,
        const AssetLoadParameters<TAsset>& parameters)
    {
        return record.has_load_parameters && record.load_parameters == parameters;
    }

    template <typename TAsset>
    void store_asset_load_parameters(
        AssetRecord<TAsset>& record,
        const AssetLoadParameters<TAsset>& parameters)
    {
        record.load_parameters = parameters;
        record.has_load_parameters = true;
    }

    template <typename TAsset>
    AssetUsage build_asset_usage(const AssetRecord<TAsset>& record)
    {
        AssetUsage usage = {};
        if (record.asset)
        {
            const auto count = record.asset.use_count();
            usage.ref_count = count <= 1 ? 0U : static_cast<uint>(count - 1);
        }
        usage.is_pinned = record.is_pinned;
        usage.stream_state = record.stream_state;
        usage.last_access = record.last_access;
        return usage;
    }

    template <typename TAsset>
    void update_asset_stream_state(AssetRecord<TAsset>& record)
    {
        if (record.stream_state != AssetStreamState::LOADING)
        {
            return;
        }
        if (!record.pending_load.valid())
        {
            return;
        }
        using namespace std::chrono_literals;
        if (record.pending_load.wait_for(0s) == std::future_status::ready)
        {
            record.stream_state = record.asset ? AssetStreamState::LOADED : AssetStreamState::UNLOADED;
            record.pending_load = {};
        }
    }

    template <typename TAsset>
    bool is_asset_record_referenced(const AssetRecord<TAsset>& record)
    {
        if (!record.asset)
        {
            return false;
        }
        return record.asset.use_count() > 1;
    }
}
