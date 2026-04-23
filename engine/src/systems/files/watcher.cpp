#include "tbx/systems/files/watcher.h"
#include <algorithm>
#include <memory>
#include <system_error>
#include <utility>

namespace tbx
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

    static FileWatchSnapshot read_snapshot(
        const IFileOps& file_ops,
        const std::filesystem::path& root)
    {
        FileWatchSnapshot snapshot = {};
        if (root.empty())
            return snapshot;

        const FileType type = file_ops.get_type(root);
        if (type == FileType::FILE)
        {
            const auto path = root.is_absolute() ? file_ops.resolve(root) : root.lexically_normal();
            snapshot.emplace(path, file_ops.get_last_write_time(root));
            return snapshot;
        }

        if (type != FileType::DIRECTORY)
            return snapshot;

        for (const auto& entry : file_ops.read_directory(root))
        {
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

    std::vector<FileWatchChange> diff_file_watch_snapshots(
        const FileWatchSnapshot& previous_snapshot,
        const FileWatchSnapshot& current_snapshot)
    {
        std::vector<FileWatchChange> changes = {};
        changes.reserve(previous_snapshot.size() + current_snapshot.size());

        for (const auto& [path, last_write_time] : current_snapshot)
        {
            const auto previous = previous_snapshot.find(path);
            if (previous == previous_snapshot.end())
            {
                changes.emplace_back(
                    FileWatchChange {
                        .path = path,
                        .type = FileWatchChangeType::CREATED,
                    });
                continue;
            }

            if (previous->second != last_write_time)
            {
                changes.emplace_back(
                    FileWatchChange {
                        .path = path,
                        .type = FileWatchChangeType::MODIFIED,
                    });
            }
        }

        for (const auto& [path, _] : previous_snapshot)
        {
            if (current_snapshot.contains(path))
                continue;

            changes.emplace_back(
                FileWatchChange {
                    .path = path,
                    .type = FileWatchChangeType::REMOVED,
                });
        }

        sort_changes(changes);
        return changes;
    }

    FileWatcher::FileWatcher(
        std::filesystem::path path_to_watch,
        FileWatchAction on_changed,
        std::chrono::milliseconds poll_interval,
        std::shared_ptr<IFileOps> file_ops)
        : _on_changed(std::move(on_changed))
        , _file_ops(std::move(file_ops))
        , _snapshot({})
        , _watched_path(path_to_watch.lexically_normal())
        , _poll_interval(
              poll_interval > std::chrono::milliseconds::zero() ? poll_interval
                                                                : std::chrono::milliseconds(250))
    {
        if (!_file_ops)
            _file_ops = std::make_shared<FileOperator>();
        if (_watched_path.empty() || !_on_changed)
            return;

        _snapshot = read_snapshot(*_file_ops, _watched_path);

        _worker = std::jthread(
            [this](std::stop_token stop_token)
            {
                run(stop_token);
            });
    }

    FileWatcher::~FileWatcher() noexcept
    {
        if (_worker.joinable())
        {
            _worker.request_stop();
            _wake_signal.notify_all();
        }
    }

    void FileWatcher::notify_changes(const std::vector<FileWatchChange>& changes) const
    {
        for (const FileWatchChange& change : changes)
        {
            _on_changed(_watched_path, change);
        }
    }

    void FileWatcher::poll_watched_path()
    {
        if (_watched_path.empty() || !_on_changed)
            return;

        FileWatchSnapshot current_snapshot = read_snapshot(*_file_ops, _watched_path);
        const std::vector<FileWatchChange> changes =
            diff_file_watch_snapshots(_snapshot, current_snapshot);

        if (!changes.empty())
            notify_changes(changes);

        _snapshot = std::move(current_snapshot);
    }

    void FileWatcher::run(std::stop_token stop_token)
    {
        std::unique_lock<std::mutex> wait_lock(_wait_mutex);
        while (!stop_token.stop_requested())
        {
            wait_lock.unlock();
            poll_watched_path();
            wait_lock.lock();

            if (stop_token.stop_requested())
                break;

            _wake_signal.wait_for(wait_lock, _poll_interval);
        }
    }
}
