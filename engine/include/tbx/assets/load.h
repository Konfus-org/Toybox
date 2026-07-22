#pragma once
#include "tbx/reflection/type_registry.h"
#include "tbx/utils/api.h"
#include "tbx/utils/result.h"
#include <filesystem>
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: Boundary internal for the generic load: reads the file as JSON and populates
    /// the object through the reflection walker (implemented next to the walker).
    TBX_API Result<void> read_registered_json(
        const std::filesystem::path& path,
        const reflection::TypeInfo& type,
        std::byte* object);

    /// @brief
    /// Purpose: THE asset-load entry: tbx::load<Texture>(path) and friends. Types with real
    /// decoders (stb, assimp, ...) implement a specialization next to their type; everything
    /// else decodes GENERICALLY — a registered type (reflection::register_type) reads as the
    /// JSON its save() writes, so plain data types never hand-write a loader.
    template <typename TAsset>
    Result<TAsset> load(const std::filesystem::path& path)
    {
        const auto type = reflection::describe<TAsset>();
        if (!type)
            return fail(
                "no load implementation for '{}' and its type is not registered",
                path.string());
        auto asset = TAsset();
        if (auto read =
                read_registered_json(path, type->get(), reinterpret_cast<std::byte*>(&asset));
            !read)
            return std::unexpected(read.error());
        return ok(std::move(asset));
    }
}
