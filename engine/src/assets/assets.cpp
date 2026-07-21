#include "tbx/assets/assets.h"
#include "tbx/core/log.h"
#include "tbx/files/files.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include <stb_image.h>

namespace tbx
{
    //// DECODERS (the static extension table — a new format is a new specialization) ////

    template <>
    Result<Texture> Assets::decode<Texture>(const std::filesystem::path& path)
    {
        auto bytes = files::read_bytes(path);
        if (!bytes)
            return std::unexpected(bytes.error());
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(bytes->data()),
            static_cast<int>(bytes->size()),
            &width,
            &height,
            &channels,
            4);
        if (!pixels)
            return fail("could not decode image '{}': {}", path.string(), stbi_failure_reason());
        auto texture = Texture {.width = width, .height = height};
        texture.pixels.assign(
            reinterpret_cast<const std::byte*>(pixels),
            reinterpret_cast<const std::byte*>(pixels) + width * height * 4);
        stbi_image_free(pixels);
        return texture;
    }

    template <>
    Result<ScriptSource> Assets::decode<ScriptSource>(const std::filesystem::path& path)
    {
        auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        return ScriptSource {.name = path.filename().string(), .source = std::move(*text)};
    }

    template <>
    Result<Json> Assets::decode<Json>(const std::filesystem::path& path)
    {
        auto text = files::read_text(path);
        if (!text)
            return std::unexpected(text.error());
        if (!is_valid_json(*text))
            return fail("'{}' is not valid JSON", path.string());
        return parse_json(*text);
    }

    //// ASSETS ////

    Assets::Assets(Jobs& jobs, Events& events)
        : _jobs(jobs)
        , _events(events)
    {
    }

    void Assets::set_root(std::filesystem::path root)
    {
        _root = std::move(root);
        // The watcher reports on its own thread; marshal to the main thread ourselves.
        _watcher.emplace(
            _root,
            [this](const std::filesystem::path& path)
            {
                _jobs.get().post_main([this, path] { handle_file_changed(path); });
            });
    }

    Result<Uuid> Assets::prepare(const std::string& relative_path)
    {
        if (_root.empty())
            return fail("asset root is not set (Assets::set_root)");
        const auto entry = _entries_by_path.find(relative_path);
        if (entry != _entries_by_path.end())
            return entry->second.id;

        // Identity-only .meta sidecar: {id, version, type}. Created on first touch so renames
        // move the id with the file instead of breaking references.
        const auto asset_path = _root / relative_path;
        const auto meta_path = asset_path.string() + ".meta";
        auto id = Uuid {};
        if (auto text = files::read_text(meta_path))
        {
            const Json meta = parse_json(*text);
            if (meta.is_object())
                id = Uuid::parse(meta.value("id", std::string()));
        }
        if (id.is_nil())
        {
            id = Uuid::generate();
            auto meta = Json {
                {"id", id.to_string()},
                {"version", 1},
                {"type", asset_path.extension().string()}};
            if (auto written = files::write_text(meta_path, dump_json(meta, 4)); !written)
                log_warn("could not write '{}': {}", meta_path, written.error());
        }
        _entries_by_path[relative_path] = Entry {.id = id, .relative_path = relative_path};
        return id;
    }

    void Assets::store(const Uuid& id, const std::string& relative_path, std::any asset)
    {
        _assets[id] = std::move(asset);
        _entries_by_path[relative_path].id = id;
    }

    std::optional<ScriptSource> Assets::get_script(const Uuid& id)
    {
        const auto it = _assets.find(id);
        if (it == _assets.end())
            return {};
        const auto* script = std::any_cast<ScriptSource>(&it->second);
        if (!script)
            return {};
        return *script;
    }

    void Assets::handle_file_changed(const std::filesystem::path& path)
    {
        auto ec = std::error_code {};
        const auto relative =
            std::filesystem::relative(path, _root, ec).generic_string();
        const auto entry = _entries_by_path.find(relative);
        if (ec || entry == _entries_by_path.end())
            return; // not a loaded asset — nothing to refresh

        // Re-decode with the same shape the asset already has, then announce.
        const Uuid id = entry->second.id;
        const auto loaded = _assets.find(id);
        if (loaded == _assets.end())
            return;
        auto refreshed = Result<std::any>(std::unexpected(std::string("unknown asset shape")));
        if (std::any_cast<Texture>(&loaded->second))
        {
            auto decoded = decode<Texture>(path);
            refreshed = decoded ? Result<std::any>(std::any(std::move(*decoded)))
                                : std::unexpected(decoded.error());
        }
        else if (std::any_cast<ScriptSource>(&loaded->second))
        {
            auto decoded = decode<ScriptSource>(path);
            refreshed = decoded ? Result<std::any>(std::any(std::move(*decoded)))
                                : std::unexpected(decoded.error());
        }
        else if (std::any_cast<Json>(&loaded->second))
        {
            auto decoded = decode<Json>(path);
            refreshed = decoded ? Result<std::any>(std::any(std::move(*decoded)))
                                : std::unexpected(decoded.error());
        }
        if (!refreshed)
        {
            log_error("hot reload of '{}' failed: {}", relative, refreshed.error());
            return;
        }
        loaded->second = std::move(*refreshed);
        _events.get().asset_reloaded.emit({.id = id});
    }
}
