#pragma once
#include "tbx/api.h"
#include "tbx/debug/log.h"
#include "tbx/files/files.h"
#include "tbx/reflection/type_registry.h"
#include "tbx/utils/result.h"
#include "tbx/serialization/registry.h"
#include <filesystem>
#include <span>
#include <string>
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: What SerializerFormat::TEXT requires of a type: the file's whole text lives in a
    /// `std::string text` member (ShaderSource, Document, ...).
    template <typename T>
    concept HasTextPayload =
        requires(T value) { requires std::is_same_v<decltype(value.text), std::string>; };

    // Boundary internals for the DEFAULT/TEXT formats, implemented next to the JSON walker
    // (json.cpp). deserialize_object/serialize_object are the whole DEFAULT round trip (payload JSON
    // plus the sidecar-routed meta fields); the meta-only pair serves TEXT, whose payload is
    // not JSON. The sidecar is `<path>.meta` — writes merge into it, preserving the identity
    // fields (id/version/type) the asset system mints.
    TBX_DLL_EXPORT Result<void> deserialize_object(
        const std::filesystem::path& path,
        const TypeInfo& type,
        std::byte* object,
        std::span<const std::string> meta_fields);
    TBX_DLL_EXPORT Result<void> serialize_object(
        const std::filesystem::path& path,
        const TypeInfo& type,
        const std::byte* object,
        std::span<const std::string> meta_fields);
    TBX_DLL_EXPORT Result<void> deserialize_meta_fields(
        const std::filesystem::path& path,
        const TypeInfo& type,
        std::byte* object,
        std::span<const std::string> meta_fields);
    TBX_DLL_EXPORT Result<void> serialize_meta_fields(
        const std::filesystem::path& path,
        const TypeInfo& type,
        const std::byte* object,
        std::span<const std::string> meta_fields);

    /// @brief
    /// Purpose: THE read entry: deserialize<Texture>(path) and friends. Dispatches
    /// on the type's registered serializer (register_serializer<T>): DEFAULT decodes through
    /// reflection, TEXT reads the file into `text`, CUSTOM calls the registered reader.
    template <typename T>
    Result<T> deserialize(const std::filesystem::path& path)
    {
        const SerializerInfo* serializer = SerializerSlot<T>::info;
        if (!serializer)
            return fail(
                "no serializer registered for '{}' (tbx::register_serializer<T> it "
                "first)",
                path.string());
        switch (serializer->format)
        {
            case SerializerFormat::DEFAULT:
            {
                const auto type = describe_type<T>();
                if (!type)
                    return fail(
                        "cannot read '{}': its type is not reflected "
                        "(tbx::register_type)",
                        path.string());
                auto value = T();
                if (auto object = deserialize_object(
                        path,
                        type->get(),
                        reinterpret_cast<std::byte*>(&value),
                        serializer->meta_fields);
                    !object)
                    return std::unexpected(object.error());
                return ok(std::move(value));
            }
            case SerializerFormat::TEXT:
            {
                if constexpr (HasTextPayload<T>)
                {
                    auto text = read_text(path);
                    if (!text)
                        return std::unexpected(text.error());
                    auto value = T();
                    value.text = std::move(*text);
                    if (!serializer->meta_fields.empty())
                    {
                        const auto type = describe_type<T>();
                        if (!type)
                            return fail(
                                "cannot read '{}' meta: its type is not reflected",
                                path.string());
                        if (auto meta = deserialize_meta_fields(
                                path,
                                type->get(),
                                reinterpret_cast<std::byte*>(&value),
                                serializer->meta_fields);
                            !meta)
                            return std::unexpected(meta.error());
                    }
                    return ok(std::move(value));
                }
                else
                    return fail(
                        "cannot read '{}': TEXT format needs a std::string text member",
                        path.string());
            }
            case SerializerFormat::CUSTOM:
            {
                if (!SerializerSlot<T>::deserializer)
                    return fail("no reader registered for '{}'", path.string());
                return SerializerSlot<T>::deserializer(path);
            }
        }
        return fail("'{}' has an unknown serializer format", path.string());
    }

    /// @brief
    /// Purpose: THE write entry: serialize(material, path) and friends. write
    /// ALWAYS means write-to-disk; a CUSTOM type registered without a writer asserts (and
    /// fails) here.
    template <typename T>
    Result<void> serialize(const T& value, const std::filesystem::path& path)
    {
        const SerializerInfo* serializer = SerializerSlot<T>::info;
        if (!serializer)
            return fail(
                "no serializer registered for '{}' (tbx::register_serializer<T> it "
                "first)",
                path.string());
        switch (serializer->format)
        {
            case SerializerFormat::DEFAULT:
            {
                const auto type = describe_type<T>();
                if (!type)
                    return fail(
                        "cannot write '{}': its type is not reflected "
                        "(tbx::register_type)",
                        path.string());
                return serialize_object(
                    path,
                    type->get(),
                    reinterpret_cast<const std::byte*>(&value),
                    serializer->meta_fields);
            }
            case SerializerFormat::TEXT:
            {
                if constexpr (HasTextPayload<T>)
                {
                    if (auto written = write_text(path, value.text); !written)
                        return written;
                    if (serializer->meta_fields.empty())
                        return ok();
                    const auto type = describe_type<T>();
                    if (!type)
                        return fail(
                            "cannot write '{}' meta: its type is not reflected",
                            path.string());
                    return serialize_meta_fields(
                        path,
                        type->get(),
                        reinterpret_cast<const std::byte*>(&value),
                        serializer->meta_fields);
                }
                else
                    return fail(
                        "cannot write '{}': TEXT format needs a std::string text member",
                        path.string());
            }
            case SerializerFormat::CUSTOM:
            {
                if (!SerializerSlot<T>::serializer)
                {
                    TBX_ASSERT(false, "no writer registered for this type");
                    return fail("no writer registered for '{}'", path.string());
                }
                return SerializerSlot<T>::serializer(value, path);
            }
        }
        return fail("'{}' has an unknown serializer format", path.string());
    }
}
