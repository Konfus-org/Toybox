#pragma once
#include "gpu_resource_cache.h"
#include "tbx/interfaces/graphics_backend.h"
#include <memory>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Reusable pool of the transient GPU buffers a frame needs. Callers hand it CPU data and
    /// a usage; it uploads the data and hands back the id. Buffers are kept and reused across frames
    /// (a frame's store() calls map to fixed pool slots by call order), so the steady state allocates
    /// no new GPU buffers — a slot only recreates its buffer when it must grow or its usage changes.
    /// @details
    /// Persistent across frames: call begin() once at the start of each frame to rewind the slot
    /// cursor before storing. A single pool is shared serially across views (each view's render fully
    /// submits before the next begins, so reusing the slots is safe). Ownership: owns (RAII) every
    /// buffer it allocates. Thread Safety: Render-lane only.
    class FrameBuffers final
    {
      public:
        explicit FrameBuffers(std::weak_ptr<IGraphicsBackend> backend);

        FrameBuffers(const FrameBuffers&) = delete;
        FrameBuffers& operator=(const FrameBuffers&) = delete;

      public:
        /// @brief Rewinds the slot cursor so the next frame's store() calls reuse this frame's
        /// buffers. Call once per frame before any store().
        void begin();

        /// @brief Uploads CPU data into the next pool slot's buffer (growing/recreating it only when
        /// needed); returns its id (INVALID_GPU_ID on failure). Passing null/zero data still yields a
        /// valid (empty) buffer.
        GpuId store(const void* data, uint64 size, BufferUsage usage);

      private:
        // One reusable pooled buffer plus the capacity and usage it was created with, so store() can
        // tell when the existing buffer still fits the request and skip recreating it.
        struct Slot
        {
            GpuResource buffer = {};
            uint64 capacity = 0U;
            BufferUsage usage = {};
        };

        std::weak_ptr<IGraphicsBackend> _backend = {};
        std::vector<Slot> _slots = {};
        size _cursor = 0U;
    };
}
