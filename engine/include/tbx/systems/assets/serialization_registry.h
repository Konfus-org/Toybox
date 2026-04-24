#pragma once
#include "tbx/systems/audio/audio_clip.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/material.h"
#include "tbx/systems/graphics/model.h"
#include "tbx/systems/graphics/shader.h"
#include "tbx/systems/graphics/texture.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

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

    template <typename TAsset>
    struct AssetPromise
    {
        std::shared_ptr<TAsset> asset = {};
        std::shared_future<Result> promise = {};
    };

    /// @brief
    /// Purpose: Stores typed serialization readers and writers and dispatches them by asset type.
    /// @details
    /// Ownership: Owns registered function objects by value; callers retain ownership of captured
    /// state. Thread Safety: Registration and lookup are synchronized internally.
    class TBX_API SerializationRegistry final
    {
      public:
        template <typename TAsset>
        using Reader = std::function<std::shared_ptr<TAsset>(
            const std::filesystem::path& asset_path,
            const AssetLoadParameters<TAsset>& parameters)>;

        template <typename TAsset>
        using AsyncReader = std::function<AssetPromise<TAsset>(
            const std::filesystem::path& asset_path,
            const AssetLoadParameters<TAsset>& parameters)>;

        template <typename TAsset>
        using Writer =
            std::function<Result(const std::filesystem::path& asset_path, const TAsset& asset)>;

      public:
        SerializationRegistry() = default;
        ~SerializationRegistry() noexcept = default;

      public:
        SerializationRegistry(const SerializationRegistry&) = delete;
        SerializationRegistry& operator=(const SerializationRegistry&) = delete;
        SerializationRegistry(SerializationRegistry&&) = delete;
        SerializationRegistry& operator=(SerializationRegistry&&) = delete;

      public:
        template <typename TAsset>
        void register_reader(Reader<TAsset> reader = {}, AsyncReader<TAsset> async_reader = {});

        template <typename TAsset>
        void deregister_reader();

        template <typename TAsset>
        bool has_reader() const;

        template <typename TAsset>
        std::shared_ptr<TAsset> read(
            const std::filesystem::path& asset_path,
            const AssetLoadParameters<TAsset>& parameters = {}) const;

        template <typename TAsset>
        AssetPromise<TAsset> read_async(
            const std::filesystem::path& asset_path,
            const AssetLoadParameters<TAsset>& parameters = {}) const;

        template <typename TAsset>
        void register_writer(Writer<TAsset> writer);

        template <typename TAsset>
        void deregister_writer();

        template <typename TAsset>
        bool has_writer() const;

        template <typename TAsset>
        Result write(const std::filesystem::path& asset_path, const TAsset& asset) const;

      private:
        struct RegistrationBase
        {
            virtual ~RegistrationBase() noexcept = default;
        };

        template <typename TAsset>
        struct Registration final : RegistrationBase
        {
            Reader<TAsset> reader = {};
            AsyncReader<TAsset> async_reader = {};
            Writer<TAsset> writer = {};
        };

      private:
        template <typename TAsset>
        Registration<TAsset>& get_or_create_registration();

        template <typename TAsset>
        std::optional<std::reference_wrapper<Registration<TAsset>>> find_registration();

        template <typename TAsset>
        std::optional<std::reference_wrapper<const Registration<TAsset>>> find_registration() const;

        template <typename TAsset>
        void erase_registration_if_empty();

        static std::shared_future<Result> make_ready_future(Result result);

      private:
        mutable std::mutex _mutex = {};
        std::unordered_map<std::type_index, std::unique_ptr<RegistrationBase>> _registrations = {};
    };

    template <typename TAsset>
    void SerializationRegistry::register_reader(
        Reader<TAsset> reader,
        AsyncReader<TAsset> async_reader)
    {
        TBX_ASSERT(
            static_cast<bool>(reader) || static_cast<bool>(async_reader),
            "Serialization registry requires a sync or async reader for type '{}'.",
            typeid(TAsset).name());

        std::lock_guard lock(_mutex);
        auto& registration = get_or_create_registration<TAsset>();
        registration.reader = std::move(reader);
        registration.async_reader = std::move(async_reader);
    }

    template <typename TAsset>
    void SerializationRegistry::deregister_reader()
    {
        std::lock_guard lock(_mutex);
        auto registration = find_registration<TAsset>();
        if (!registration.has_value())
            return;

        registration->get().reader = {};
        registration->get().async_reader = {};
        erase_registration_if_empty<TAsset>();
    }

    template <typename TAsset>
    bool SerializationRegistry::has_reader() const
    {
        std::lock_guard lock(_mutex);
        const auto registration = find_registration<TAsset>();
        if (!registration.has_value())
            return false;

        return static_cast<bool>(registration->get().reader)
               || static_cast<bool>(registration->get().async_reader);
    }

    template <typename TAsset>
    std::shared_ptr<TAsset> SerializationRegistry::read(
        const std::filesystem::path& asset_path,
        const AssetLoadParameters<TAsset>& parameters) const
    {
        Reader<TAsset> reader = {};
        AsyncReader<TAsset> async_reader = {};

        {
            std::lock_guard lock(_mutex);
            const auto registration = find_registration<TAsset>();
            if (registration.has_value())
            {
                reader = registration->get().reader;
                async_reader = registration->get().async_reader;
            }
        }

        TBX_ASSERT(
            static_cast<bool>(reader) || static_cast<bool>(async_reader),
            "Serialization reader not registered for type '{}'.",
            typeid(TAsset).name());
        if (reader)
            return reader(asset_path, parameters);
        if (!async_reader)
            return {};

        auto promise = async_reader(asset_path, parameters);
        if (promise.promise.valid())
            promise.promise.wait();
        return promise.asset;
    }

    template <typename TAsset>
    AssetPromise<TAsset> SerializationRegistry::read_async(
        const std::filesystem::path& asset_path,
        const AssetLoadParameters<TAsset>& parameters) const
    {
        Reader<TAsset> reader = {};
        AsyncReader<TAsset> async_reader = {};

        {
            std::lock_guard lock(_mutex);
            const auto registration = find_registration<TAsset>();
            if (registration.has_value())
            {
                reader = registration->get().reader;
                async_reader = registration->get().async_reader;
            }
        }

        TBX_ASSERT(
            static_cast<bool>(reader) || static_cast<bool>(async_reader),
            "Serialization reader not registered for type '{}'.",
            typeid(TAsset).name());
        if (async_reader)
            return async_reader(asset_path, parameters);

        auto result = AssetPromise<TAsset> {};
        if (!reader)
            return result;

        result.asset = reader(asset_path, parameters);
        auto read_result = Result {};
        if (result.asset)
            read_result.flag_success();
        else
            read_result.flag_failure(
                std::string("Serialization reader returned no asset for '") + asset_path.string()
                + "'.");
        result.promise = make_ready_future(std::move(read_result));
        return result;
    }

    template <typename TAsset>
    void SerializationRegistry::register_writer(Writer<TAsset> writer)
    {
        TBX_ASSERT(
            static_cast<bool>(writer),
            "Serialization registry requires a writer for type '{}'.",
            typeid(TAsset).name());

        std::lock_guard lock(_mutex);
        auto& registration = get_or_create_registration<TAsset>();
        registration.writer = std::move(writer);
    }

    template <typename TAsset>
    void SerializationRegistry::deregister_writer()
    {
        std::lock_guard lock(_mutex);
        auto registration = find_registration<TAsset>();
        if (!registration.has_value())
            return;

        registration->get().writer = {};
        erase_registration_if_empty<TAsset>();
    }

    template <typename TAsset>
    bool SerializationRegistry::has_writer() const
    {
        std::lock_guard lock(_mutex);
        const auto registration = find_registration<TAsset>();
        return registration.has_value() && static_cast<bool>(registration->get().writer);
    }

    template <typename TAsset>
    Result SerializationRegistry::write(
        const std::filesystem::path& asset_path,
        const TAsset& asset) const
    {
        Writer<TAsset> writer = {};

        {
            std::lock_guard lock(_mutex);
            const auto registration = find_registration<TAsset>();
            if (registration.has_value())
                writer = registration->get().writer;
        }

        TBX_ASSERT(
            static_cast<bool>(writer),
            "Serialization writer not registered for type '{}'.",
            typeid(TAsset).name());
        if (!writer)
        {
            auto result = Result {};
            result.flag_failure(
                std::string("Serialization writer not registered for type '")
                + typeid(TAsset).name() + "'.");
            return result;
        }

        return writer(asset_path, asset);
    }

    template <typename TAsset>
    SerializationRegistry::Registration<TAsset>& SerializationRegistry::get_or_create_registration()
    {
        auto key = std::type_index(typeid(TAsset));
        auto iterator = _registrations.find(key);
        if (iterator == _registrations.end())
        {
            iterator = _registrations.emplace(key, std::make_unique<Registration<TAsset>>()).first;
        }

        return static_cast<Registration<TAsset>&>(*iterator->second);
    }

    template <typename TAsset>
    std::optional<std::reference_wrapper<SerializationRegistry::Registration<TAsset>>> SerializationRegistry::
        find_registration()
    {
        auto iterator = _registrations.find(std::type_index(typeid(TAsset)));
        if (iterator == _registrations.end())
            return std::nullopt;

        return std::ref(static_cast<Registration<TAsset>&>(*iterator->second));
    }

    template <typename TAsset>
    std::optional<std::reference_wrapper<const SerializationRegistry::Registration<TAsset>>> SerializationRegistry::
        find_registration() const
    {
        auto iterator = _registrations.find(std::type_index(typeid(TAsset)));
        if (iterator == _registrations.end())
            return std::nullopt;

        return std::cref(static_cast<const Registration<TAsset>&>(*iterator->second));
    }

    template <typename TAsset>
    void SerializationRegistry::erase_registration_if_empty()
    {
        auto iterator = _registrations.find(std::type_index(typeid(TAsset)));
        if (iterator == _registrations.end())
            return;

        const auto& registration = static_cast<const Registration<TAsset>&>(*iterator->second);
        if (registration.reader || registration.async_reader || registration.writer)
            return;

        _registrations.erase(iterator);
    }

    inline std::shared_future<Result> SerializationRegistry::make_ready_future(Result result)
    {
        std::promise<Result> promise = {};
        promise.set_value(std::move(result));
        return promise.get_future().share();
    }
}
