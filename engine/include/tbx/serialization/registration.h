#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/debug/log.h"
#include "tbx/reflection/type_registry.h"
#include "tbx/serialization/read_write.h"
#include "tbx/serialization/registry.h"
#include <any>
#include <concepts>
#include <string>
#include <typeinfo>
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: Fluent registration builder — the disk half of a type's registration, like
    /// reflection's TypeRegistration is the shape half:
    /// tbx::register_serializer<Material>().format(SerializerFormat::DEFAULT)
    ///     .meta(&Material::some_property)...
    /// DEFAULT round-trips through the type's reflection (so the type must be
    /// register_type'd first); TEXT reads/writes the file as raw text through a
    /// `std::string text` member; CUSTOM takes .deserializer()/.serializer() functions — omit the
    /// writer and write<T> asserts. Types deriving tbx::Asset get the hot-reload facet
    /// stamped automatically.
    template <typename T>
    class Registration final
    {
      public:
        explicit Registration()
            : _info(get_serializer_registry().add(make_info()))
        {
            SerializerSlot<T>::info = &_info.get();
            stamp_facets();
        }

      public:
        /// @brief
        /// Purpose: Declares how the type moves between memory and disk. DEFAULT errors when
        /// the type has no reflection; TEXT errors when it has no `std::string text` member.
        Registration& format(const SerializerFormat format)
        {
            _info.get().format = format;
            if (format == SerializerFormat::DEFAULT && !describe_type<T>())
            {
                TBX_ERROR(
                    "'{}' registered with SerializerFormat::DEFAULT but is not reflected — "
                    "tbx::register_type it first",
                    _info.get().name);
                TBX_ASSERT(false, "SerializerFormat::DEFAULT needs reflection");
            }
            if constexpr (!HasTextPayload<T>)
            {
                if (format == SerializerFormat::TEXT)
                {
                    TBX_ERROR(
                        "'{}' registered with SerializerFormat::TEXT but has no std::string text member",
                        _info.get().name);
                    TBX_ASSERT(false, "SerializerFormat::TEXT needs a text member");
                }
            }
            return *this;
        }

        /// @brief
        /// Purpose: Routes one reflected property into the `<path>.meta` sidecar instead of
        /// the payload file — sidecar data rides with the asset's identity.
        template <typename TField>
        Registration& meta(TField T::* member)
        {
            const auto type = describe_type<T>();
            if (!type)
            {
                TBX_ERROR("'{}' .meta needs reflection — register_type it first", _info.get().name);
                TBX_ASSERT(false, ".meta needs reflection");
                return *this;
            }
            // Offset via a live instance instead of the null-deref trick — no UB.
            auto probe = T();
            const auto offset = static_cast<size>(
                reinterpret_cast<const char*>(&(probe.*member))
                - reinterpret_cast<const char*>(&probe));
            for (const FieldInfo& field : type->get().fields)
            {
                if (field.offset != offset)
                    continue;
                _info.get().meta_fields.push_back(field.name);
                return *this;
            }
            TBX_ERROR("'{}' .meta member is not a reflected field", _info.get().name);
            TBX_ASSERT(false, ".meta member must be a reflected field");
            return *this;
        }

        /// @brief
        /// Purpose: The CUSTOM-format read function — a plain free function next to the type
        /// (gpu_deserialize_texture, ...).
        Registration& deserializer(Result<T> (*read_file)(const std::filesystem::path&))
        {
            SerializerSlot<T>::deserializer = read_file;
            return *this;
        }

        /// @brief
        /// Purpose: The CUSTOM-format write function; omit it and write<T> asserts.
        Registration& serializer(Result<void> (*write_file)(const T&, const std::filesystem::path&))
        {
            SerializerSlot<T>::serializer = write_file;
            return *this;
        }

      private:
        static SerializerInfo make_info()
        {
            auto info = SerializerInfo {};
            info.type_hash = typeid(T).hash_code();
            const auto type = describe_type<T>();
            info.name = type ? type->get().name : typeid(T).name();
            return info;
        }

        void stamp_facets()
        {
            if constexpr (std::derived_from<T, Asset>)
            {
                SerializerInfo& info = _info.get();
                info.asset_shape = typeid(T).hash_code();
                info.deserialize_asset = [](const std::filesystem::path& disk_path,
                                     const Uuid& id,
                                     const std::string& relative_path) -> Result<std::any>
                {
                    auto decoded = deserialize<T>(disk_path);
                    if (!decoded)
                        return std::unexpected(decoded.error());
                    decoded->id = id;
                    decoded->path = relative_path;
                    return std::any(std::move(*decoded));
                };
            }
        }

      private:
        std::reference_wrapper<SerializerInfo> _info;
    };

    /// @brief
    /// Purpose: Registers how type T reads/writes on disk; chain .format()/.meta()/.deserializer()/
    /// .serializer() off the result. Registering a type twice keeps the one existing record.
    template <typename T>
    Registration<T> register_serializer()
    {
        return Registration<T>();
    }
}
