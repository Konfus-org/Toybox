#include "tbx/files/filesystem.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace tbx
{
    static std::filesystem::path get_repo_root_directory()
    {
#if defined(TBX_REPO_ROOT)
        std::filesystem::path root(TBX_REPO_ROOT);
        if (!root.empty())
        {
            return root.lexically_normal();
        }
#endif
        return {};
    }

    static std::filesystem::path get_current_path()
    {
        std::error_code ec;
        auto current = std::filesystem::current_path(ec);
        if (ec)
            return {};
        return current;
    }

    static std::filesystem::path get_default_working_directory()
    {
        return get_current_path();
    }

    static std::filesystem::path resolve_with_working(
        const std::filesystem::path& working_directory,
        const std::filesystem::path& path)
    {
        if (path.empty())
            return {};
        if (path.is_absolute() || working_directory.empty())
            return path.lexically_normal();
        return (working_directory / path).lexically_normal();
    }

    static std::filesystem::path choose_directory(
        const std::filesystem::path& working_directory,
        const std::filesystem::path& candidate_path,
        const std::string& default_path)
    {
        if (!candidate_path.empty())
            return resolve_with_working(working_directory, candidate_path);
        return (working_directory / default_path).lexically_normal();
    }

    static bool is_path_prefix(
        const std::filesystem::path& root,
        const std::filesystem::path& candidate)
    {
        if (root.empty() || candidate.empty())
        {
            return false;
        }

        const auto normalized_root = root.lexically_normal();
        const auto normalized_candidate = candidate.lexically_normal();
        auto root_iter = normalized_root.begin();
        auto root_end = normalized_root.end();
        auto candidate_iter = normalized_candidate.begin();
        auto candidate_end = normalized_candidate.end();

        for (; root_iter != root_end; ++root_iter, ++candidate_iter)
        {
            if (candidate_iter == candidate_end || *candidate_iter != *root_iter)
            {
                return false;
            }
        }

        return true;
    }

    FileSystem::FileSystem(
        const std::filesystem::path& working_directory,
        const std::filesystem::path& plugins_directory,
        const std::filesystem::path& logs_directory,
        const std::vector<std::filesystem::path>& asset_directories)
        : _working_directory(working_directory.lexically_normal())
        , _plugins_directory(plugins_directory)
        , _logs_directory(logs_directory)
    {
        if (_working_directory.empty()) // use default working dir if none specified
            _working_directory = get_default_working_directory();

        _plugins_directory = choose_directory(
            _working_directory,
            _plugins_directory,
            "./"); // choose between user-defined and default plugins dir
        _logs_directory = choose_directory(
            _working_directory,
            _logs_directory,
            "logs"); // choose between user-defined and default logs dir

        const auto repo_root = get_repo_root_directory();
        if (!repo_root.empty() && is_path_prefix(repo_root, _working_directory))
            add_assets_directory(repo_root / "resources"); // built-in assets
        for (const auto& extra : asset_directories)
            add_assets_directory(extra); // user-defined assets
        if (asset_directories.empty())
        {
            add_assets_directory(
                resolve_with_working(_working_directory, "assets")); // default assets dir
        }
    }

    std::filesystem::path FileSystem::get_working_directory() const
    {
        return _working_directory;
    }

    std::filesystem::path FileSystem::get_plugins_directory() const
    {
        return _plugins_directory;
    }

    std::filesystem::path FileSystem::get_logs_directory() const
    {
        return _logs_directory;
    }

    std::vector<std::filesystem::path> FileSystem::get_assets_directories() const
    {
        return _assets_directories;
    }

    void FileSystem::add_assets_directory(const std::filesystem::path& path)
    {
        const auto resolved = resolve_with_working(_working_directory, path);
        if (resolved.empty())
        {
            return;
        }

        const auto normalized = resolved.lexically_normal();
        const auto is_duplicate = std::any_of(
            _assets_directories.begin(),
            _assets_directories.end(),
            [&normalized](const std::filesystem::path& existing)
            {
                return existing == normalized;
            });
        if (is_duplicate)
        {
            return;
        }

        _assets_directories.push_back(normalized);
    }

    std::filesystem::path FileSystem::resolve_asset_path(const std::filesystem::path& path) const
    {
        if (path.empty())
        {
            return {};
        }
        if (path.is_absolute())
        {
            return path;
        }

        for (const auto& root : _assets_directories)
        {
            if (root.empty())
            {
                continue;
            }
            const auto candidate = resolve_relative_path(root / path);
            if (exists(candidate))
            {
                return candidate;
            }
        }

        return resolve_relative_path(path);
    }

    std::filesystem::path FileSystem::resolve_relative_path(const std::filesystem::path& path) const
    {
        return resolve_with_working(_working_directory, path);
    }

    bool FileSystem::exists(const std::filesystem::path& path) const
    {
        const std::filesystem::path resolved = resolve_relative_path(path);
        std::error_code ec;
        return std::filesystem::exists(resolved, ec) && !ec;
    }

    FilePathType FileSystem::get_file_type(const std::filesystem::path& path) const
    {
        const std::filesystem::path resolved = resolve_relative_path(path);
        std::error_code ec;
        const auto status = std::filesystem::status(resolved, ec);
        if (ec)
            return FilePathType::None;
        if (status.type() == std::filesystem::file_type::regular)
            return FilePathType::Regular;
        if (status.type() == std::filesystem::file_type::directory)
            return FilePathType::Directory;
        return FilePathType::Other;
    }

    std::vector<std::filesystem::path> FileSystem::read_directory(
        const std::filesystem::path& root) const
    {
        std::vector<std::filesystem::path> entries;
        const std::filesystem::path resolved_root = resolve_relative_path(root);
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(resolved_root, ec), end;
             it != end && !ec;
             ++it)
        {
            entries.emplace_back(it->path());
        }
        return entries;
    }

    bool FileSystem::create_directory(const std::filesystem::path& path)
    {
        const std::filesystem::path resolved = resolve_relative_path(path);
        std::error_code ec;
        std::filesystem::create_directories(resolved, ec);
        return !ec;
    }

    bool FileSystem::create_file(const std::filesystem::path& path)
    {
        const std::filesystem::path resolved = resolve_relative_path(path);
        std::error_code ec;
        auto std_path = std::filesystem::path(resolved);
        if (std_path.has_parent_path())
            std::filesystem::create_directories(std_path.parent_path(), ec);
        if (ec)
            return false;
        std::ofstream stream(std_path, std::ios::out | std::ios::trunc);
        return stream.is_open();
    }

    bool FileSystem::read_file(
        const std::filesystem::path& path,
        FileDataFormat format,
        std::string& out) const
    {
        const std::filesystem::path resolved = resolve_relative_path(path);
        const bool binary = format == FileDataFormat::Binary;
        std::ios_base::openmode mode = std::ios::in;
        if (binary)
            mode |= std::ios::binary;

        auto std_path = std::filesystem::path(resolved);
        auto stream = std::ifstream(std_path, mode);
        if (!stream.is_open())
            return false;

        auto contents =
            std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

        // Strip UTF-8 BOM for text mode; binary payloads are left untouched.
        if (!binary && contents.size() >= 3)
        {
            const unsigned char bom0 = static_cast<unsigned char>(contents[0]);
            const unsigned char bom1 = static_cast<unsigned char>(contents[1]);
            const unsigned char bom2 = static_cast<unsigned char>(contents[2]);
            if (bom0 == 0xEF && bom1 == 0xBB && bom2 == 0xBF)
                contents.erase(0, 3);
        }

        out = contents;
        return true;
    }

    bool FileSystem::write_file(
        const std::filesystem::path& path,
        FileDataFormat format,
        const std::string& data)
    {
        const std::filesystem::path resolved = resolve_relative_path(path);
        const bool binary = format == FileDataFormat::Binary;
        std::ios_base::openmode mode = std::ios::out | std::ios::trunc;
        if (binary)
            mode |= std::ios::binary;

        auto std_path = std::filesystem::path(resolved);
        std::ofstream stream(std_path, mode);
        if (!stream.is_open())
            return false;

        stream.write(data.data(), static_cast<std::streamsize>(data.size()));
        return stream.good();
    }

    bool FileSystem::remove(const std::filesystem::path& path)
    {
        const std::filesystem::path resolved = resolve_relative_path(path);
        std::error_code ec;
        auto std_path = std::filesystem::path(resolved);
        std::filesystem::remove(std_path, ec);
        return !ec;
    }

    bool FileSystem::rename(const std::filesystem::path& from, const std::filesystem::path& to)
    {
        if (!copy(from, to))
            return false;
        return remove(from);
    }

    bool FileSystem::copy(const std::filesystem::path& from, const std::filesystem::path& to)
    {
        const std::filesystem::path resolved_from = resolve_relative_path(from);
        const std::filesystem::path resolved_to = resolve_relative_path(to);
        auto std_path_from = std::filesystem::path(resolved_from);
        auto std_path_to = std::filesystem::path(resolved_to);
        std::error_code ec;
        std::filesystem::copy_file(
            std_path_from,
            std_path_to,
            std::filesystem::copy_options::overwrite_existing,
            ec);
        return !ec;
    }
}
