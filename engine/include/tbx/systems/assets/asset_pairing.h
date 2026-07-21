#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>

namespace tbx::asset_pairing
{
    /// @brief
    /// The single place the engine expresses how on-disk files pair into one logical asset, so the
    /// rules aren't re-derived per call site. Two pairings exist:
    ///   - metadata: an asset payload `<file>` pairs with its sidecar `<file>.meta`. This holds for
    ///     every asset, scripts included — a script's payload is its `<header>.h` and its sidecar is
    ///     `<header>.h.meta`.
    ///   - source: a C++ header (`.h`/`.hpp`/`.hh`) pairs with its implementation (`.cpp`/`.cc`/`.cxx`).
    /// Studio mirrors these rules in its own AssetPairing helper (file operations run editor-side).

    /// The sidecar suffix appended to an asset payload to name its metadata file.
    inline constexpr std::string_view metadata_suffix = ".meta";

    /// C++ header extensions and their implementation companions — the rule that pairs a script's
    /// header (its asset payload) with its source so the two travel together (rename/delete/duplicate).
    inline constexpr std::array<std::string_view, 3> source_header_extensions = {".h", ".hpp", ".hh"};
    inline constexpr std::array<std::string_view, 3> source_impl_extensions = {".cpp", ".cc", ".cxx"};

    /// True when a path names a C++ header (`.h`/`.hpp`/`.hh`) — the payload of a script asset. The
    /// comparison is case-insensitive.
    [[nodiscard]]
    inline bool is_source_header(const std::filesystem::path& path)
    {
        auto extension = path.extension().string();
        std::ranges::transform(
            extension,
            extension.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return std::ranges::find(source_header_extensions, extension) != source_header_extensions.end();
    }

    /// The metadata sidecar for an asset payload: `<asset>.meta`.
    [[nodiscard]]
    inline std::filesystem::path metadata_path(const std::filesystem::path& asset_path)
    {
        auto meta = asset_path;
        meta += std::string(metadata_suffix);
        return meta;
    }
}
