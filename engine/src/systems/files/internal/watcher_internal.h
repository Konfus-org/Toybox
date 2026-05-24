#pragma once
#include "tbx/systems/files/watcher.h"
#include <algorithm>
#include <system_error>

namespace tbx::internal
{
    static std::filesystem::path get_snapshot_path(
        const IFileOps& file_ops,
        const std::filesystem::path& watched_path,
        const std::filesystem::path& resolved_path)
    {
        if (watched_path.is_absolute())
            return resolved_path.lexically_normal();

        std::error_code error = {};
        const auto relative_path =
            std::filesystem::relative(resolved_path, file_ops.get_working_directory(), error);
        if (error)
        {
            return resolved_path.lexically_normal();
        }

        return relative_path.lexically_normal();
    }

    static FileWatchOptions make_fixed_file_watch_options(std::chrono::milliseconds poll_interval)
    {
        const auto normalized_interval = poll_interval > std::chrono::milliseconds::zero()
                                             ? poll_interval
                                             : std::chrono::milliseconds(250);
        return FileWatchOptions {
            .active_poll_interval = normalized_interval,
            .idle_poll_interval = normalized_interval,
            .unchanged_scan_threshold = 0U,
        };
    }

    static FileWatchOptions normalize_file_watch_options(FileWatchOptions options)
    {
        if (options.active_poll_interval <= std::chrono::milliseconds::zero())
            options.active_poll_interval = std::chrono::milliseconds(250);
        if (options.idle_poll_interval <= std::chrono::milliseconds::zero())
            options.idle_poll_interval = options.active_poll_interval;
        return options;
    }

    static FileWatchSnapshot read_snapshot(
        const IFileOps& file_ops,
        const std::filesystem::path& root,
        const FileWatchFilter& filter)
    {
        FileWatchSnapshot snapshot = {};
        if (root.empty())
            return snapshot;

        const FileType type = file_ops.get_type(root);
        if (type == FileType::FILE)
        {
            if (filter && !filter(root))
                return snapshot;

            const auto path = root.is_absolute() ? file_ops.resolve(root) : root.lexically_normal();
            snapshot.emplace(path, file_ops.get_last_write_time(root));
            return snapshot;
        }

        if (type != FileType::DIRECTORY)
            return snapshot;

        for (const auto& entry : file_ops.read_directory(root))
        {
            if (filter && !filter(entry))
                continue;

            if (file_ops.get_type(entry) != FileType::FILE)
                continue;

            const auto resolved_entry = file_ops.resolve(entry);
            snapshot.emplace(
                get_snapshot_path(file_ops, root, resolved_entry),
                file_ops.get_last_write_time(entry));
        }

        return snapshot;
    }

    static void sort_changes(std::vector<FileWatchChange>& changes)
    {
        std::sort(
            changes.begin(),
            changes.end(),
            [](const FileWatchChange& lhs, const FileWatchChange& rhs)
            {
                const std::string lhs_path = lhs.path.generic_string();
                const std::string rhs_path = rhs.path.generic_string();
                if (lhs_path == rhs_path)
                {
                    return static_cast<int>(lhs.type) < static_cast<int>(rhs.type);
                }

                return lhs_path < rhs_path;
            });
    }
}
