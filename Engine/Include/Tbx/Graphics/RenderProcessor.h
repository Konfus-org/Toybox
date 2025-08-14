#pragma once
#include "Tbx/TBS/Box.h"
#include "Tbx/Graphics/Buffers.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    /// This processes a box for rendering.
    /// It takes all the toys in the box and converts them to render data for the render pipeline to consume.
    /// </summary>
    class RenderProcessor
    {
    public:
        /// <summary>
        /// Gets render data required to setup boxes such as compiling shaders.
        /// </summary>
        static FrameBuffer PreProcess(const std::vector < std::shared_ptr<Box>>& boxes);

        /// <summary>
        /// Returns render data required to render boxes.
        /// </summary>
        static FrameBuffer Process(const std::vector<std::shared_ptr<Box>>& boxes);

    private:
        static void PreProcessToy(const ToyHandle& toy, const std::shared_ptr<Box>& box, FrameBuffer& buffer);
        static void ProcessToy(const ToyHandle& toy, const std::shared_ptr<Box>& box, FrameBuffer& buffer);
    };
}