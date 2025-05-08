#pragma once
#include <Tbx/Core/Rendering/RenderQueue.h>
#include <Tbx/Core/TBS/Toy.h>
#include <Tbx/Core/TBS/PlaySpace.h>
#include <memory>

namespace Tbx
{
    /// <summary>
    /// This processes a playSpace for rendering.
    /// It takes all the boxes and toys in the playSpace and converts them to render data for the render pipeline to consume.
    /// </summary>
    class RenderProcessor
    {
    public:
        /// <summary>
        /// Gets render data required to setup playSpace such as compiling shaders.
        /// </summary>
        const RenderBatch& PreProcess(const std::shared_ptr<PlaySpace>& playSpace);

        /// <summary>
        /// Returns render data required to render playSpace.
        /// </summary>
        const RenderBatch& Process(const std::shared_ptr<PlaySpace>& playSpace);

    private:

        void PreProcessToy(const Toy& toy, const std::shared_ptr<PlaySpace>& playSpace);

        void ProcessToy(const Toy& toy, const std::shared_ptr<PlaySpace>& playSpace);

        RenderBatch _currBatch = {};
    };
}