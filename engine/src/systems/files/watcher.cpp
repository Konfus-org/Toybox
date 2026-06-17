#include "tbx/systems/files/watcher.h"
#include <system_error>

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
        std::weak_ptr<IFileOps> file_ops)
        : FileWatcher(
              std::move(path_to_watch),
              std::move(on_changed),
              make_fixed_file_watch_options(poll_interval),
              std::move(file_ops))
    {
    }

    FileWatcher::FileWatcher(
        std::filesystem::path path_to_watch,
        FileWatchAction on_changed,
        FileWatchOptions options,
        std::weak_ptr<IFileOps> file_ops)
        : _on_changed(std::move(on_changed))
        , _file_ops(std::move(file_ops))
        , _snapshot({})
        , _options(normalize_file_watch_options(std::move(options)))
        , _watched_path(path_to_watch.lexically_normal())
    {
        auto file_ops_service = lock_file_ops();
        if (!file_ops_service)
        {
            _owned_file_ops = std::make_shared<FileOperator>();
            _file_ops = _owned_file_ops;
            file_ops_service = _owned_file_ops;
        }
        if (_watched_path.empty() || !_on_changed)
            return;

        _snapshot = read_snapshot(*file_ops_service, _watched_path, _options.filter);

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
            _worker.join();
        }
    }

    void FileWatcher::notify_changes(const std::vector<FileWatchChange>& changes) const
    {
        for (const FileWatchChange& change : changes)
        {
            _on_changed(_watched_path, change);
        }
    }

    std::chrono::milliseconds FileWatcher::get_next_poll_interval() const
    {
        if (_options.idle_poll_interval <= _options.active_poll_interval)
            return _options.active_poll_interval;
        if (_unchanged_scan_count < _options.unchanged_scan_threshold)
            return _options.active_poll_interval;

        return _options.idle_poll_interval;
    }

    std::shared_ptr<IFileOps> FileWatcher::lock_file_ops() const
    {
        return _file_ops.lock();
    }

    bool FileWatcher::poll_watched_path()
    {
        if (_watched_path.empty() || !_on_changed)
            return false;

        const auto file_ops = lock_file_ops();
        if (!file_ops)
            return false;

        FileWatchSnapshot current_snapshot =
            read_snapshot(*file_ops, _watched_path, _options.filter);
        const std::vector<FileWatchChange> changes =
            diff_file_watch_snapshots(_snapshot, current_snapshot);

        if (!changes.empty())
            notify_changes(changes);

        _snapshot = std::move(current_snapshot);
        if (changes.empty())
        {
            _unchanged_scan_count += 1U;
            return false;
        }

        _unchanged_scan_count = 0U;
        return true;
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

            _wake_signal.wait_for(wait_lock, get_next_poll_interval());
        }
    }
}
