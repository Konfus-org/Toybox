#pragma once
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include <unordered_map>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Tracks renderer resource usage age without owning GPU resources or asset data.
    /// @details
    /// Ownership: Stores copied resource ids and last-use ages only.
    /// Thread Safety: Not inherently thread-safe; call from the render lane.
    class TBX_API RenderingResourceTracker final
    {
      public:
        using ResourceCollection = std::vector<uint>;

      public:
        RenderingResourceTracker() = default;
        ~RenderingResourceTracker() = default;

      public:
        RenderingResourceTracker(const RenderingResourceTracker&) = delete;
        RenderingResourceTracker& operator=(const RenderingResourceTracker&) = delete;
        RenderingResourceTracker(RenderingResourceTracker&&) noexcept = default;
        RenderingResourceTracker& operator=(RenderingResourceTracker&&) noexcept = default;

      public:
        /// @brief
        /// Purpose: Returns tracked resource ids.
        const ResourceCollection& get_tracked_resources() const;

        /// @brief
        /// Purpose: Returns seconds since the resource was last tracked.
        float get_time_alive(uint resource) const;

        /// @brief
        /// Purpose: Returns true when a resource is currently tracked.
        bool is_tracked(uint resource) const;

        /// @brief
        /// Purpose: Marks a resource as used and resets its age.
        void track(uint resource);

        /// @brief
        /// Purpose: Removes a resource from tracking after the renderer unloads it.
        void untrack(uint resource);

        /// @brief
        /// Purpose: Advances tracked resource ages.
        void update(DeltaTime delta);

      private:
        ResourceCollection _tracked_resources = {};
        std::unordered_map<uint, float> _time_alive = {};
    };
}
