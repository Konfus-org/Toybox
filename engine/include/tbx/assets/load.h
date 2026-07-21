#pragma once
#include "tbx/utils/api.h"
#include "tbx/utils/result.h"
#include <filesystem>

namespace tbx
{
    /// @brief
    /// Purpose: THE asset-load entry: tbx::load<Texture>(path) and friends. Every asset type
    /// implements its own specialization next to its type (gfx/texture.cpp, audio/
    /// audio_clip.cpp, ...) — specialized third-party decoders (stb, assimp) stay inside
    /// those .cpp files and are never public. A type without an implementation fails.
    template <typename TAsset>
    Result<TAsset> load(const std::filesystem::path& path)
    {
        return fail("no load implementation for asset '{}'", path.string());
    }
}
