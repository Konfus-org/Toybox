#pragma once
#include <Tbx/Core/Rendering/RenderQueue.h>
#include <Tbx/Core/TBS/Toy.h>
#include <Tbx/Core/TBS/PlaySpace.h>
#include <memory>

namespace Tbx
{
    /// <summary>
    /// This processes a playspace for rendering.
    /// It takes all the boxes and toys in the playspace and converts them to render data for the render pipeline to consume.
    /// </summary>
    class RenderProcessor
    {
    public:
        /// <summary>
        /// Gets render data required to setup playspace such as compiling shaders.
        /// </summary>
        const RenderBatch& PreProcess(const std::shared_ptr<PlaySpace>& playspace);

        /// <summary>
        /// Returns render data required to render playspace.
        /// </summary>
        const RenderBatch& Process(const std::shared_ptr<PlaySpace>& playspace);

    private:

        void PreProcessToy(const Toy& toy, const std::shared_ptr<PlaySpace>& playspace);

        void ProcessToy(const Toy& toy, const std::shared_ptr<PlaySpace>& playspace);

        RenderBatch _currBatch = {};
    };
}