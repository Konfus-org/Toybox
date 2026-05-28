#include "tbx/systems/files/in_memory_file_ops.h"
#include "tbx/systems/files/watcher.h"
#include <chrono>
#include <condition_variable>
#include <thread>

namespace tbx::tests::file_system
{
    struct CapturedFileWatchChange
    {
        std::filesystem::path watched_path = {};
        std::filesystem::path changed_path = {};
        FileWatchChangeType change_type = FileWatchChangeType::MODIFIED;
    };

    class CapturingFileWatchAction final
    {
      public:
        void operator()(const std::filesystem::path& watched_path, const FileWatchChange& change)
        {
            {
                std::lock_guard<std::mutex> lock(_changes_mutex);
                _changes.emplace_back(
                    CapturedFileWatchChange {
                        .watched_path = watched_path,
                        .changed_path = change.path,
                        .change_type = change.type,
                    });
            }

            _changes_signal.notify_all();
        }

      public:
        std::vector<CapturedFileWatchChange> get_changes() const
        {
            std::lock_guard<std::mutex> lock(_changes_mutex);
            return _changes;
        }

        bool wait_for_count(
            std::vector<CapturedFileWatchChange>::size_type count,
            std::chrono::milliseconds timeout) const
        {
            std::unique_lock<std::mutex> lock(_changes_mutex);
            return _changes_signal.wait_for(
                lock,
                timeout,
                [this, count]()
                {
                    return _changes.size() >= count;
                });
        }

      private:
        mutable std::mutex _changes_mutex = {};
        mutable std::condition_variable _changes_signal = {};
        mutable std::vector<CapturedFileWatchChange> _changes = {};
    };

    class CountingFileOps final : public IFileOps
    {
      public:
        struct FileEntry
        {
            std::filesystem::file_time_type last_write_time =
                std::filesystem::file_time_type::clock::now();
        };

      public:
        CountingFileOps(std::filesystem::path working_directory)
            : _working_directory(std::move(working_directory))
        {
        }

      public:
        std::filesystem::path get_working_directory() const override
        {
            return _working_directory;
        }

        std::filesystem::path resolve(const std::filesystem::path& path) const override
        {
            if (path.is_absolute())
                return path.lexically_normal();

            return (_working_directory / path).lexically_normal();
        }

        bool exists(const std::filesystem::path& path) const override
        {
            std::lock_guard lock(_files_mutex);
            return _files.contains(resolve(path));
        }

        FileType get_type(const std::filesystem::path& path) const override
        {
            {
                std::lock_guard lock(_query_mutex);
                _get_type_queries[resolve(path).generic_string()] += 1U;
            }

            std::lock_guard lock(_files_mutex);
            const auto resolved = resolve(path);
            if (_files.contains(resolved))
                return FileType::FILE;

            for (const auto& [file_path, _] : _files)
            {
                if (is_within_directory(file_path, resolved))
                    return FileType::DIRECTORY;
            }

            return FileType::NONE;
        }

        std::filesystem::file_time_type get_last_write_time(
            const std::filesystem::path& path) const override
        {
            {
                std::lock_guard lock(_query_mutex);
                _last_write_time_queries[resolve(path).generic_string()] += 1U;
            }

            std::lock_guard lock(_files_mutex);
            const auto file = _files.find(resolve(path));
            if (file == _files.end())
                return {};

            return file->second.last_write_time;
        }

        std::vector<std::filesystem::path> read_directory(
            const std::filesystem::path& root) const override
        {
            {
                std::lock_guard lock(_query_mutex);
                _read_directory_count += 1U;
            }

            std::lock_guard lock(_files_mutex);
            const auto resolved_root = resolve(root);
            auto entries = std::vector<std::filesystem::path>();
            for (const auto& [file_path, _] : _files)
            {
                if (is_within_directory(file_path, resolved_root))
                    entries.push_back(file_path);
            }

            return entries;
        }

        bool read_file(const std::filesystem::path&, FileDataFormat, std::string&) const override
        {
            return false;
        }

        bool write_file(const std::filesystem::path& path, FileDataFormat, const std::string&)
            override
        {
            write_file_entry(path, std::filesystem::file_time_type::clock::now());
            return true;
        }

      public:
        uint get_last_write_time_query_count(const std::filesystem::path& path) const
        {
            std::lock_guard lock(_query_mutex);
            const auto query = _last_write_time_queries.find(resolve(path).generic_string());
            return query == _last_write_time_queries.end() ? 0U : query->second;
        }

        uint get_read_directory_count() const
        {
            std::lock_guard lock(_query_mutex);
            return _read_directory_count;
        }

        uint get_type_query_count(const std::filesystem::path& path) const
        {
            std::lock_guard lock(_query_mutex);
            const auto query = _get_type_queries.find(resolve(path).generic_string());
            return query == _get_type_queries.end() ? 0U : query->second;
        }

        bool wait_for_read_directory_count(
            const uint count,
            const std::chrono::milliseconds timeout) const
        {
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            while (std::chrono::steady_clock::now() < deadline)
            {
                if (get_read_directory_count() >= count)
                    return true;

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            return get_read_directory_count() >= count;
        }

        void erase(const std::filesystem::path& path)
        {
            std::lock_guard lock(_files_mutex);
            _files.erase(resolve(path));
        }

        void touch(
            const std::filesystem::path& path,
            const std::filesystem::file_time_type last_write_time)
        {
            std::lock_guard lock(_files_mutex);
            _files[resolve(path)].last_write_time = last_write_time;
        }

        void write_file_entry(
            const std::filesystem::path& path,
            const std::filesystem::file_time_type last_write_time)
        {
            std::lock_guard lock(_files_mutex);
            _files[resolve(path)] = FileEntry {.last_write_time = last_write_time};
        }

      private:
        static bool is_within_directory(
            const std::filesystem::path& path,
            const std::filesystem::path& directory)
        {
            const auto normalized_path = path.lexically_normal();
            const auto normalized_directory = directory.lexically_normal();
            const auto mismatch = std::mismatch(
                normalized_directory.begin(),
                normalized_directory.end(),
                normalized_path.begin(),
                normalized_path.end());
            return mismatch.first == normalized_directory.end();
        }

      private:
        std::filesystem::path _working_directory = {};
        mutable std::mutex _files_mutex = {};
        std::unordered_map<std::filesystem::path, FileEntry> _files = {};

        mutable std::mutex _query_mutex = {};
        mutable std::unordered_map<std::string, uint> _get_type_queries = {};
        mutable std::unordered_map<std::string, uint> _last_write_time_queries = {};
        mutable uint _read_directory_count = 0U;
    };

    TEST(FileWatcherTests, DiffsCreatedModifiedAndRemovedFiles)
    {
        const auto base_time = std::filesystem::file_time_type::clock::now();
        const FileWatchSnapshot previous_snapshot = {
            {std::filesystem::path("assets/a.txt"), base_time},
            {std::filesystem::path("assets/b.txt"), base_time},
        };
        const FileWatchSnapshot current_snapshot = {
            {std::filesystem::path("assets/a.txt"), base_time + std::chrono::seconds(1)},
            {std::filesystem::path("assets/c.txt"), base_time},
        };

        const std::vector<FileWatchChange> changes =
            diff_file_watch_snapshots(previous_snapshot, current_snapshot);

        ASSERT_EQ(changes.size(), 3u);
        EXPECT_EQ(changes[0].path, std::filesystem::path("assets/a.txt"));
        EXPECT_EQ(changes[0].type, FileWatchChangeType::MODIFIED);
        EXPECT_EQ(changes[1].path, std::filesystem::path("assets/b.txt"));
        EXPECT_EQ(changes[1].type, FileWatchChangeType::REMOVED);
        EXPECT_EQ(changes[2].path, std::filesystem::path("assets/c.txt"));
        EXPECT_EQ(changes[2].type, FileWatchChangeType::CREATED);
    }

    TEST(FileWatcherTests, InvokesChangeActionForFilesWithinWatchedDirectory)
    {
        const auto base_time = std::filesystem::file_time_type::clock::now();
        auto file_ops = std::make_shared<InMemoryFileOps>("/virtual");
        file_ops->write_file_entry("assets/a.txt", {}, base_time);
        file_ops->write_file_entry("assets/b.txt", {}, base_time);
        CapturingFileWatchAction on_changed = {};

        FileWatcher watcher(
            "assets",
            [&on_changed](const std::filesystem::path& watched_path, const FileWatchChange& change)
            {
                on_changed(watched_path, change);
            },
            std::chrono::milliseconds(5),
            file_ops);

        file_ops->touch("assets/a.txt", base_time + std::chrono::seconds(1));
        file_ops->erase("assets/b.txt");
        file_ops->write_file_entry("assets/c.txt", {}, base_time);

        ASSERT_TRUE(on_changed.wait_for_count(3, std::chrono::milliseconds(500)));

        std::vector<CapturedFileWatchChange> changes = on_changed.get_changes();
        ASSERT_EQ(changes.size(), 3u);

        std::sort(
            changes.begin(),
            changes.end(),
            [](const CapturedFileWatchChange& lhs, const CapturedFileWatchChange& rhs)
            {
                return lhs.changed_path.generic_string() < rhs.changed_path.generic_string();
            });

        EXPECT_EQ(changes[0].watched_path, std::filesystem::path("assets"));
        EXPECT_EQ(changes[0].changed_path, std::filesystem::path("assets/a.txt"));
        EXPECT_EQ(changes[0].change_type, FileWatchChangeType::MODIFIED);

        EXPECT_EQ(changes[1].watched_path, std::filesystem::path("assets"));
        EXPECT_EQ(changes[1].changed_path, std::filesystem::path("assets/b.txt"));
        EXPECT_EQ(changes[1].change_type, FileWatchChangeType::REMOVED);

        EXPECT_EQ(changes[2].watched_path, std::filesystem::path("assets"));
        EXPECT_EQ(changes[2].changed_path, std::filesystem::path("assets/c.txt"));
        EXPECT_EQ(changes[2].change_type, FileWatchChangeType::CREATED);
    }

    TEST(FileWatcherTests, ExistingConstructorKeepsFixedPollingCadence)
    {
        // Arrange
        const auto base_time = std::filesystem::file_time_type::clock::now();
        auto file_ops = std::make_shared<CountingFileOps>("/virtual");
        file_ops->write_file_entry("assets/a.txt", base_time);
        CapturingFileWatchAction on_changed = {};

        // Act
        FileWatcher watcher(
            "assets",
            [&on_changed](const std::filesystem::path& watched_path, const FileWatchChange& change)
            {
                on_changed(watched_path, change);
            },
            std::chrono::milliseconds(5),
            file_ops);

        // Assert
        EXPECT_TRUE(file_ops->wait_for_read_directory_count(6U, std::chrono::milliseconds(200)));
        EXPECT_TRUE(on_changed.get_changes().empty());
    }

    TEST(FileWatcherTests, AdaptiveOptionsBackOffAfterUnchangedPolls)
    {
        // Arrange
        const auto base_time = std::filesystem::file_time_type::clock::now();
        auto file_ops = std::make_shared<CountingFileOps>("/virtual");
        file_ops->write_file_entry("assets/a.txt", base_time);
        CapturingFileWatchAction on_changed = {};

        // Act
        FileWatcher watcher(
            "assets",
            [&on_changed](const std::filesystem::path& watched_path, const FileWatchChange& change)
            {
                on_changed(watched_path, change);
            },
            FileWatchOptions {
                .active_poll_interval = std::chrono::milliseconds(5),
                .idle_poll_interval = std::chrono::milliseconds(200),
                .unchanged_scan_threshold = 2U,
            },
            file_ops);

        // Assert
        ASSERT_TRUE(file_ops->wait_for_read_directory_count(3U, std::chrono::milliseconds(200)));
        const uint count_after_backoff = file_ops->get_read_directory_count();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EXPECT_LE(file_ops->get_read_directory_count(), count_after_backoff + 1U);
        EXPECT_TRUE(on_changed.get_changes().empty());
    }

    TEST(FileWatcherTests, AdaptiveOptionsReturnToActivePollingAfterChange)
    {
        // Arrange
        const auto base_time = std::filesystem::file_time_type::clock::now();
        auto file_ops = std::make_shared<CountingFileOps>("/virtual");
        file_ops->write_file_entry("assets/a.txt", base_time);
        CapturingFileWatchAction on_changed = {};
        FileWatcher watcher(
            "assets",
            [&on_changed](const std::filesystem::path& watched_path, const FileWatchChange& change)
            {
                on_changed(watched_path, change);
            },
            FileWatchOptions {
                .active_poll_interval = std::chrono::milliseconds(5),
                .idle_poll_interval = std::chrono::milliseconds(50),
                .unchanged_scan_threshold = 2U,
            },
            file_ops);
        ASSERT_TRUE(file_ops->wait_for_read_directory_count(3U, std::chrono::milliseconds(200)));

        // Act
        file_ops->touch("assets/a.txt", base_time + std::chrono::seconds(1));

        // Assert
        ASSERT_TRUE(on_changed.wait_for_count(1U, std::chrono::milliseconds(500)));
        const uint count_after_change = file_ops->get_read_directory_count();
        EXPECT_TRUE(file_ops->wait_for_read_directory_count(
            count_after_change + 2U,
            std::chrono::milliseconds(80)));
    }

    TEST(FileWatcherTests, FilterSkipsIgnoredPathQueriesAndCallbacks)
    {
        // Arrange
        const auto base_time = std::filesystem::file_time_type::clock::now();
        auto file_ops = std::make_shared<CountingFileOps>("/virtual");
        file_ops->write_file_entry("assets/a.txt", base_time);
        file_ops->write_file_entry("assets/ignored.tmp", base_time);
        CapturingFileWatchAction on_changed = {};

        // Act
        FileWatcher watcher(
            "assets",
            [&on_changed](const std::filesystem::path& watched_path, const FileWatchChange& change)
            {
                on_changed(watched_path, change);
            },
            FileWatchOptions {
                .active_poll_interval = std::chrono::milliseconds(5),
                .idle_poll_interval = std::chrono::milliseconds(5),
                .filter =
                    [](const std::filesystem::path& path)
                {
                    return path.extension() == ".txt";
                },
            },
            file_ops);
        file_ops->touch("assets/ignored.tmp", base_time + std::chrono::seconds(1));

        // Assert
        ASSERT_TRUE(file_ops->wait_for_read_directory_count(3U, std::chrono::milliseconds(200)));
        EXPECT_EQ(file_ops->get_type_query_count("assets/ignored.tmp"), 0U);
        EXPECT_EQ(file_ops->get_last_write_time_query_count("assets/ignored.tmp"), 0U);
        EXPECT_FALSE(on_changed.wait_for_count(1U, std::chrono::milliseconds(30)));
    }
}
