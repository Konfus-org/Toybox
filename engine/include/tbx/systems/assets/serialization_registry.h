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
    /// @brief
    /// Purpose: Provides default read parameters for serialized asset types without custom options.
    /// @details
    /// Ownership: Value type settings owned by the caller.
    /// Thread Safety: Safe to copy between threads.
    struct DefaultAssetLoadParameters
    {
        bool operator==(const DefaultAssetLoadParameters& other) const = default;
    };

    // TODO: move the explicit load params next to the asset structs/classes
    /// @brief
    /// Purpose: Provides texture-specific read parameters for serialized texture assets.
    /// @details
    /// Ownership: Value type settings owned by the caller.
    /// Thread Safety: Safe to copy between threads.
    struct TextureLoadParameters
    {
        Texture texture = Texture(
            Size(1, 1),
            TextureWrap::REPEAT,
            TextureFilter::LINEAR,
            TextureFormat::RGBA,
            TextureMipmaps::ENABLED,
            TextureCompression::AUTO,
            std::vector<Pixel> {255, 255, 255, 255});

        bool operator==(const TextureLoadParameters& other) const = default;
    };

    /// @brief
    /// Purpose: Provides model-specific read parameters for serialized model assets.
    /// @details
    /// Ownership: Value type settings owned by the caller.
    /// Thread Safety: Safe to copy between threads.
    struct ModelLoadParameters
    {
        bool operator==(const ModelLoadParameters& other) const = default;
    };

    /// @brief
    /// Purpose: Provides shader-specific read parameters for serialized shader assets.
    /// @details
    /// Ownership: Value type settings owned by the caller.
    /// Thread Safety: Safe to copy between threads.
    struct ShaderLoadParameters
    {
        bool operator==(const ShaderLoadParameters& other) const = default;
    };

    /// @brief
    /// Purpose: Provides material-specific read parameters for serialized material assets.
    /// @details
    /// Ownership: Value type settings owned by the caller.
    /// Thread Safety: Safe to copy between threads.
    struct MaterialLoadParameters
    {
        bool operator==(const MaterialLoadParameters& other) const = default;
    };

    /// @brief
    /// Purpose: Provides audio-specific read parameters for serialized audio assets.
    /// @details
    /// Ownership: Value type settings owned by the caller.
    /// Thread Safety: Safe to copy between threads.
    struct AudioLoadParameters
    {
        bool operator==(const AudioLoadParameters& other) const = default;
    };

    // TODO: Remove the tratis and use the params directly with some base like struct
    // TextureLoadParams : public LoadParams<Texture> {...}
    template <typename TAsset>
    struct AssetSerializationTraits
    {
        using Parameters = DefaultAssetLoadParameters;
    };

    template <>
    struct AssetSerializationTraits<Texture>
    {
        using Parameters = TextureLoadParameters;
    };

    template <>
    struct AssetSerializationTraits<Model>
    {
        using Parameters = ModelLoadParameters;
    };

    template <>
    struct AssetSerializationTraits<Shader>
    {
        using Parameters = ShaderLoadParameters;
    };

    template <>
    struct AssetSerializationTraits<Material>
    {
        using Parameters = MaterialLoadParameters;
    };

    template <>
    struct AssetSerializationTraits<AudioClip>
    {
        using Parameters = AudioLoadParameters;
    };

    template <typename TAsset>
    using AssetLoadParameters = typename AssetSerializationTraits<TAsset>::Parameters;

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
        static Result try_read_tbx_asset_common_meta(
            const Json& data,
            const std::filesystem::path& meta_path,
            uint32 expected_version,
            AssetLoadMetadata& out_metadata);

        static void apply_tbx_asset_common_meta(const AssetLoadMetadata& metadata, Asset& asset);
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

        Result try_read_tbx_serialized_asset_meta(
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

