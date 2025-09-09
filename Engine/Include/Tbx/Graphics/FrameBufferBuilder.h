#pragma once
#include "Tbx/TBS/Box.h"
#include "Tbx/Graphics/Buffers.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    /// Builds frame buffers of render commands from boxes and their toys.
    /// </summary>
    class FrameBufferBuilder
    {
    public:
        /// <summary>
        /// Generates commands to upload resources for the provided boxes.
        /// </summary>
        FrameBuffer BuildUploadBuffer(const std::vector<std::shared_ptr<Box>>& boxes);

        /// <summary>
        /// Generates commands necessary to render the provided boxes.
        /// </summary>
        FrameBuffer BuildRenderBuffer(const std::vector<std::shared_ptr<Box>>& boxes);

    private:
        void AddToyUploadCommandsToBuffer(const ToyHandle& toy, const std::shared_ptr<Box>& box, FrameBuffer& buffer);
        void AddToyRenderCommandsToBuffer(const ToyHandle& toy, const std::shared_ptr<Box>& box, FrameBuffer& buffer);
    };
}
