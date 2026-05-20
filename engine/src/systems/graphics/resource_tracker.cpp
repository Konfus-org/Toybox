#include "tbx/systems/graphics/resource_tracker.h"
#include <algorithm>

namespace tbx
{
    const RenderingResourceTracker::ResourceCollection&
        RenderingResourceTracker::get_tracked_resources() const
    {
        return _tracked_resources;
    }

    float RenderingResourceTracker::get_time_alive(const uint resource) const
    {
        const auto iterator = _time_alive.find(resource);
        return iterator == _time_alive.end() ? 0.0F : iterator->second;
    }

    bool RenderingResourceTracker::is_tracked(const uint resource) const
    {
        return _time_alive.contains(resource);
    }

    void RenderingResourceTracker::track(const uint resource)
    {
        if (resource == 0U)
            return;

        if (!is_tracked(resource))
            _tracked_resources.push_back(resource);

        _time_alive[resource] = 0.0F;
    }

    void RenderingResourceTracker::untrack(const uint resource)
    {
        _time_alive.erase(resource);
        const auto iterator =
            std::remove(_tracked_resources.begin(), _tracked_resources.end(), resource);
        _tracked_resources.erase(iterator, _tracked_resources.end());
    }

    void RenderingResourceTracker::update(const DeltaTime delta)
    {
        const auto seconds = static_cast<float>(delta.seconds);
        for (auto& entry : _time_alive)
            entry.second += seconds;
    }
}
