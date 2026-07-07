#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/assets/audio_clip.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/shader.h"
#include "tbx/types/assets/texture.h"
#include "tbx/utils/result.h"
#include <concepts>
#include <future>
#include <typeindex>

namespace tbx
{
    // Per-asset load parameters live next to their asset structs (see e.g. TextureLoadParameters in
    // texture.h); AssetLoadParameters<T> resolves them via the load_parameters_of overloads declared
    // in asset.h.

    /// @brief
    /// Purpose: Carries sidecar metadata parsed before asset payload loading.
    /// @details
    /// Ownership: Value type copied into loader and transformer calls.
    /// Thread Safety: Safe to copy between threads.
    struct AssetLoadMetadata
    {
        Uuid id = {};
        bool polymorphic = false;
        uint32 version = 0U;
    };

    template <typename TAsset>
    struct AssetPromise
    {
        std::shared_ptr<TAsset> asset = {};
        AssetLoadMetadata metadata = {};
        std::shared_future<Result> promise = {};
    };

    template <typename TAsset>
    struct AssetReadResult
    {
        std::shared_ptr<TAsset> asset = {};
        AssetLoadMetadata metadata = {};
        // The registered asset-type name resolved while reading (e.g. "Material"). Lets a type-erased
        // caller round-trip the asset without re-deriving its type. Empty on failure.
        std::string type_name = {};
        Result result = Result();
    };

    /// @brief
    /// Purpose: Stores typed serialization loaders, transformers, and writers by asset type.
    /// @details
    /// Ownership: Owns registered function objects by value; callers retain ownership of captured
    /// state. Thread Safety: Registration and lookup are synchronized internally.
    class TBX_API SerializationRegistry final
    {
      public:
        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        using Loader = std::function<Result(
            const std::filesystem::path& asset_path,
            const AssetLoadParameters<TAsset>& parameters,
            const AssetLoadMetadata& metadata,
            TAsset& asset)>;

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        using AsyncLoader = std::function<std::shared_future<Result>(
            const std::filesystem::path& asset_path,
            const AssetLoadParameters<TAsset>& parameters,
            AssetLoadMetadata metadata,
            const std::shared_ptr<TAsset>& asset)>;

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        using Transformer = std::function<Result(
            const std::filesystem::path& asset_path,
            const AssetLoadParameters<TAsset>& parameters,
            const AssetLoadMetadata& metadata,
            TAsset& asset)>;

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        using Writer =
            std::function<Result(const std::filesystem::path& asset_path, const TAsset& asset)>;

      public:
        SerializationRegistry();
        explicit SerializationRegistry(std::weak_ptr<IFileOps> file_ops);
        ~SerializationRegistry() noexcept = default;

      public:
        SerializationRegistry(const SerializationRegistry&) = delete;
        SerializationRegistry& operator=(const SerializationRegistry&) = delete;
        SerializationRegistry(SerializationRegistry&&) = delete;
        SerializationRegistry& operator=(SerializationRegistry&&) = delete;

      public:
        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        void register_loader(Loader<TAsset> loader = {}, AsyncLoader<TAsset> async_loader = {});

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        void deregister_loader();

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        bool has_loader() const;

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        void register_transformer(Transformer<TAsset> transformer);

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        void deregister_transformer();

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        bool has_transformer() const;

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        bool can_read() const;

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        AssetReadResult<TAsset> read_result(
            const std::filesystem::path& asset_path,
            const AssetLoadParameters<TAsset>& parameters = {}) const;

        AssetReadResult<Asset> read_registered_asset_result(
            const std::filesystem::path& asset_path) const;

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        std::shared_ptr<TAsset> read(
            const std::filesystem::path& asset_path,
            const AssetLoadParameters<TAsset>& parameters = {}) const;

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        AssetPromise<TAsset> read_async(
            const std::filesystem::path& asset_path,
            const AssetLoadParameters<TAsset>& parameters = {}) const;

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        void register_writer(Writer<TAsset> writer);

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        void deregister_writer();

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        bool has_writer() const;

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        Result write(const std::filesystem::path& asset_path, const TAsset& asset) const;

        // Type-erased write for callers that only hold a runtime AssetTypeRegistration and a pointer to the
        // matching asset (e.g. saving an asset chosen by type name from the editor). Mirrors the
        // registered-body branch of the templated write.
        Result write(
            const std::filesystem::path& asset_path,
            const AssetTypeRegistration& asset_registration,
            const void* asset) const;

      private:
        struct RegistrationBase
        {
            virtual ~RegistrationBase() noexcept = default;
        };

        template <typename TAsset>
        struct Registration final : RegistrationBase
        {
            Loader<TAsset> loader = {};
            AsyncLoader<TAsset> async_loader = {};
            std::vector<Transformer<TAsset>> transformers = {};
            Writer<TAsset> writer = {};
        };

      private:
        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        Registration<TAsset>& get_or_create_registration();

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        std::optional<std::reference_wrapper<Registration<TAsset>>> find_registration();

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        std::optional<std::reference_wrapper<const Registration<TAsset>>> find_registration() const;

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        void erase_registration_if_empty();

        static std::shared_future<Result> make_ready_future(Result result);
        static Result make_failed_result(std::string report);
        static Result try_read_asset_common_meta(
            const Json& data,
            const std::filesystem::path& meta_path,
            uint32 expected_version,
            AssetLoadMetadata& out_metadata);

        static void apply_asset_common_meta(const AssetLoadMetadata& metadata, Asset& asset);
        std::shared_ptr<IFileOps> lock_file_ops() const;

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        static void prepend_default_meta_transformers(
            const std::optional<AssetTypeRegistration>& asset_registration,
            const std::optional<std::string>& meta_data,
            bool loaded_meta,
            std::vector<Transformer<TAsset>>& transformers);

        static Result try_load_registered_asset_body(
            const std::filesystem::path& asset_path,
            const IFileOps& file_ops,
            const AssetTypeRegistration& asset_registration,
            void* asset);

        static Result try_write_registered_asset_body(
            const std::filesystem::path& asset_path,
            IFileOps& file_ops,
            const AssetTypeRegistration& asset_registration,
            const void* asset);

        // Writes an asset's flat .meta sidecar (asset_path + ".meta") from its [[meta]] fields. Used for
        // meta-only assets (e.g. textures) which have no body file. Merges over any existing meta so legacy
        // keys survive, and never lets a save drop the asset's identity (a missing/zero id is restored from
        // the existing meta).
        static Result try_write_registered_asset_meta(
            const std::filesystem::path& asset_path,
            IFileOps& file_ops,
            const AssetTypeRegistration& asset_registration,
            const void* asset);

        // Writes a self-describing script meta (`*.h.meta`): identity only (id/version/type), never a
        // serialized body — so saving a script asset cannot clobber its identity.
        static Result write_self_describing_script_meta(
            IFileOps& file_ops,
            const std::filesystem::path& asset_path,
            const AssetTypeRegistration& asset_registration);

        Result try_read_serialized_asset_meta(
            const std::filesystem::path& asset_path,
            const IFileOps& file_ops,
            uint32 expected_version,
            AssetLoadMetadata& out_metadata,
            std::optional<std::string>& out_meta_data,
            bool& out_loaded_meta) const;

      private:
        mutable std::mutex _mutex = {};
        std::shared_ptr<IFileOps> _owned_file_ops = nullptr;
        std::weak_ptr<IFileOps> _file_ops = {};
        std::unordered_map<std::type_index, std::unique_ptr<RegistrationBase>> _registrations = {};
    };
}

#include "tbx/systems/assets/serialization_registry.inl"

