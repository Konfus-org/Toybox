#pragma once
#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace tbx::asset_pairing
{
    /// @brief
    /// The single place the engine expresses how on-disk files pair into one logical asset, so the
    /// rules aren't re-derived per call site. Two pairings exist:
    ///   - metadata: an asset payload `<file>` pairs with its sidecar `<file>.meta`. A self-describing
    ///     metadata file (a script's `<header>.h.meta`) is the exception — it IS the asset and its own
    ///     metadata, with no separate payload.
    ///   - source: a C++ header (`.h`/`.hpp`/`.hh`) pairs with its implementation (`.cpp`/`.cc`/`.cxx`).
    /// Studio mirrors these rules in its own AssetPairing helper (file operations run editor-side).

    /// The sidecar suffix appended to an asset payload to name its metadata file.
    inline constexpr std::string_view metadata_suffix = ".meta";

    /// C++ header extensions and their implementation companions — the rule that pairs a script's
    /// header with its source so the two travel together (rename/delete/duplicate).
    inline constexpr std::array<std::string_view, 3> source_header_extensions = {".h", ".hpp", ".hh"};
    inline constexpr std::array<std::string_view, 3> source_impl_extensions = {".cpp", ".cc", ".cxx"};

    /// True when a path names a self-describing metadata file (`*.h.meta` / `*.hpp.meta` / `*.hh.meta`):
    /// the meta IS the asset (a script), carrying only identity, with no separate payload file. The check
    /// is on the trailing suffix only, so it is path-separator agnostic.
    [[nodiscard]]
    inline bool is_self_describing_metadata(std::string_view path)
    {
        constexpr std::string_view header_meta_suffixes[] = {".h.meta", ".hpp.meta", ".hh.meta"};
        return std::ranges::any_of(
            header_meta_suffixes,
            [path](std::string_view suffix)
            {
                return path.size() > suffix.size() && path.ends_with(suffix);
            });
    }

    /// True for an ordinary `<asset>.meta` sidecar — metadata for a separate payload file, NOT an asset
    /// in its own right (so the registry tracks the payload, not this). A self-describing `*.h.meta` is
    /// excluded because it IS the asset.
    [[nodiscard]]
    inline bool is_metadata_sidecar(std::string_view path)
    {
        return path.size() > metadata_suffix.size() && path.ends_with(metadata_suffix)
               && !is_self_describing_metadata(path);
    }

    /// The metadata file for an asset payload. A self-describing meta is returned unchanged (it is its
    /// own metadata); every other asset's metadata lives at `<asset>.meta`.
    [[nodiscard]]
    inline std::filesystem::path metadata_path(const std::filesystem::path& asset_path)
    {
        if (is_self_describing_metadata(asset_path.generic_string()))
            return asset_path;

        auto meta = asset_path;
        meta += std::string(metadata_suffix);
        return meta;
    }
}
