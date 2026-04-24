#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/assets/fallbacks.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/utils/result.h"
#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <typeinfo>
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
        std::shared_ptr<IFileOps> _file_ops = nullptr;
        std::vector<std::filesystem::path> _asset_directories = {};
        std::unordered_map<std::string, AssetRegistryEntry> _entries_by_path = {};
        std::unordered_map<Uuid, std::string> _path_by_id = {};
    };

    struct AssetStoreReloadResult
    {
        bool attempted = false;
        bool succeeded = true;
    };

    struct IAssetStore
    {
        virtual ~IAssetStore() = default;
        virtual std::string_view get_asset_type_name() const = 0;
        virtual void erase(Uuid asset_id) = 0;
        virtual AssetStoreReloadResult reload(
            const AssetRegistryEntry& entry,
            std::chrono::steady_clock::time_point timestamp,
            const SerializationRegistry& serialization_registry) = 0;
        virtual uint unload_unreferenced() = 0;
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

        void erase(Uuid asset_id) override
        {
            records.erase(asset_id);
        }

        std::string_view get_asset_type_name() const override
        {
            return typeid(TAsset).name();
        }

        AssetStoreReloadResult reload(
            const AssetRegistryEntry& entry,
            const std::chrono::steady_clock::time_point timestamp,
            const SerializationRegistry& serialization_registry) override
        {
            auto iterator = records.find(entry.asset_id);
            if (iterator == records.end())
            {
                return {};
            }

            auto& record = iterator->second;

            auto parameters = record.has_load_parameters ? record.load_parameters
                                                         : AssetLoadParameters<TAsset> {};
            auto promise =
                serialization_registry.read_async<TAsset>(entry.resolved_path, parameters);
            if (!promise.asset)
            {
                promise.asset = make_fallback_asset<TAsset>(parameters);
                if (promise.asset && !promise.promise.valid())
                {
                    auto result = Result {};
                    result.flag_failure("Primary asset read failed. Using fallback asset.");
                    std::promise<Result> completion = {};
                    completion.set_value(std::move(result));
                    promise.promise = completion.get_future().share();
                }
            }
            record.asset = std::move(promise.asset);
            record.pending_load = promise.promise;
            record.load_parameters = parameters;
            record.has_load_parameters = true;
            record.stream_state =
                record.asset ? AssetStreamState::LOADING : AssetStreamState::UNLOADED;
            record.last_access = timestamp;
            if (record.stream_state == AssetStreamState::LOADING && record.pending_load.valid())
            {
                if (record.pending_load.wait_for(std::chrono::seconds(0))
                    == std::future_status::ready)
                {
                    record.stream_state =
                        record.asset ? AssetStreamState::LOADED : AssetStreamState::UNLOADED;
                    record.pending_load = {};
                }
            }

            auto result = AssetStoreReloadResult {
                .attempted = true,
                .succeeded = record.asset != nullptr,
            };
            return result;
        }

        uint unload_unreferenced() override
        {
            uint unloaded_count = 0U;
            for (auto& entry : records)
            {
                auto& record = entry.second;
                if (!record.is_pinned && record.asset && record.asset.use_count() <= 1)
                {
                    record.asset.reset();
                    record.stream_state = AssetStreamState::UNLOADED;
                    unloaded_count += 1U;
                }
            }
            return unloaded_count;
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
            record.stream_state =
                record.asset ? AssetStreamState::LOADED : AssetStreamState::UNLOADED;
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
