#pragma once
#include "Tbx/Systems/TBS/Playspace.h"
#include "Tbx/Graphics/Buffers.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    /// This processes a playSpace for rendering.
    /// It takes all the toys in the playSpace and converts them to render data for the render pipeline to consume.
    /// </summary>
    class RenderProcessor
    {
    public:
        /// <summary>
        /// Gets render data required to setup playSpace such as compiling shaders.
        /// </summary>
        static FrameBuffer PreProcess(const std::vector < std::shared_ptr<Playspace>>& playspaces);

        /// <summary>
        /// Returns render data required to render playSpace.
        /// </summary>
        static FrameBuffer Process(const std::vector<std::shared_ptr<Playspace>>& playspaces);

    private:
        static void PreProcessToy(const Toy& toy, const std::shared_ptr<Playspace>& playSpace, FrameBuffer& buffer);
        static void ProcessToy(const Toy& toy, const std::shared_ptr<Playspace>& playSpace, FrameBuffer& buffer);
    };
}