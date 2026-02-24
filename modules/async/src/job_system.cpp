#include "tbx/async/job_system.h"
#include <stdexcept>

namespace tbx
{
    namespace
    {
        std::size_t resolve_worker_count(std::size_t configured_worker_count)
        {
            if (configured_worker_count > 0)
                return configured_worker_count;

            auto detected_worker_count =
                static_cast<std::size_t>(std::thread::hardware_concurrency());

            if (detected_worker_count == 0)
                return 1;

            return detected_worker_count;
        }
    }

    JobSystem::JobSystem(const JobSystemConfiguration& configuration)
    {
        auto worker_count = resolve_worker_count(configuration.worker_count);
        _workers.reserve(worker_count);

        for (std::size_t index = 0; index < worker_count; ++index)
        {
            _workers.emplace_back(
                [this](std::stop_token stop_token)
                {
                    run_worker(stop_token);
                });
        }
    }

    JobSystem::~JobSystem() noexcept
    {
        stop();
    }

    void JobSystem::schedule(Job&& job)
    {
        if (!job)
            return;

        {
            auto lock = std::scoped_lock(_queue_mutex);

            if (!_accepting_jobs)
                throw std::runtime_error("Cannot schedule a job after stop().");

            _queued_jobs.push_back(std::move(job));
        }

        _queued_job_signal.notify_one();
    }

    void JobSystem::wait_for_idle()
    {
        auto lock = std::unique_lock(_queue_mutex);
        _idle_signal.wait(
            lock,
            [this]()
            {
                return _queued_jobs.empty() && _active_jobs == 0;
            });
    }

    void JobSystem::stop()
    {
        {
            auto lock = std::scoped_lock(_queue_mutex);

            if (!_accepting_jobs && _workers.empty())
                return;

            _accepting_jobs = false;
        }

        _queued_job_signal.notify_all();

        for (auto& worker : _workers)
            worker.request_stop();

        _queued_job_signal.notify_all();

        for (auto& worker : _workers)
            if (worker.joinable())
                worker.join();

        _workers.clear();

        _idle_signal.notify_all();
    }

    std::size_t JobSystem::get_worker_count() const
    {
        auto lock = std::scoped_lock(_queue_mutex);
        return _workers.size();
    }

    void JobSystem::run_worker(std::stop_token stop_token)
    {
        while (true)
        {
            Job job = {};

            {
                auto lock = std::unique_lock(_queue_mutex);
                _queued_job_signal.wait(
                    lock,
                    [this, stop_token]()
                    {
                        return stop_token.stop_requested() || !_queued_jobs.empty()
                               || !_accepting_jobs;
                    });

                if (_queued_jobs.empty())
                {
                    if (stop_token.stop_requested() || !_accepting_jobs)
                        return;

                    continue;
                }

                job = std::move(_queued_jobs.front());
                _queued_jobs.pop_front();
                _active_jobs += 1;
            }

            try
            {
                job();
            }
            catch (...)
            {
                // Fire-and-forget jobs have no return channel for exceptions.
            }

            {
                auto lock = std::scoped_lock(_queue_mutex);
                _active_jobs -= 1;

                if (_queued_jobs.empty() && _active_jobs == 0)
                    _idle_signal.notify_all();
            }
        }
    }
}
