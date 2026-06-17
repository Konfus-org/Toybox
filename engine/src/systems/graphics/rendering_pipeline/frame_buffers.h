#pragma once
#include "gpu_resource_cache.h"
#include "tbx/interfaces/graphics_backend.h"
#include <memory>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Simple generic storage for the GPU buffers a single frame needs. Callers hand it CPU
    /// data and a usage; it uploads the data into a transient buffer and hands back the id. It owns
    /// every buffer for the lifetime of the frame and frees them all when it is destroyed.
    /// @details
    /// Constructed per frame and dropped at the end of the frame (RAII), so there is no reset/begin
    /// step and no per-resource cleanup. Thread Safety: Render-lane only.
    class FrameBuffers final
    {
      public:
        explicit FrameBuffers(std::weak_ptr<IGraphicsBackend> backend);

        FrameBuffers(const FrameBuffers&) = delete;
        FrameBuffers& operator=(const FrameBuffers&) = delete;

      public:
        /// @brief Uploads CPU data into a new buffer that lives for this frame; returns its id
        /// (INVALID_GPU_ID on failure). Passing null/zero data still allocates an empty buffer.
        GpuId store(const void* data, uint64 size, BufferUsage usage);

      private:
        std::weak_ptr<IGraphicsBackend> _backend = {};
        std::vector<GpuResource> _buffers = {};
    };
}
