#pragma once
#include <Tbx/Core/Rendering/RenderQueue.h>
#include <Tbx/Core/Rendering/Shader.h>
#include <Tbx/Core/Rendering/Material.h>
#include <Tbx/Core/Rendering/Camera.h>
#include <Tbx/Core/Rendering/Mesh.h>
#include <Tbx/Core/TBS/Toy.h>
#include <Tbx/Core/TBS/Playspace.h>
#include <Tbx/Core/Math/Transform.h>
#include <vector>
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
        const RenderBatch& PreProcess(const std::weak_ptr<Playspace>& playspace);

        /// <summary>
        /// Returns render data required to render playspace.
        /// </summary>
        const RenderBatch& Process(const std::weak_ptr<Playspace>& playspace);

    private:

        void PreProcessToy(const Toy& toy, const std::weak_ptr<Playspace>& playspace);

        void ProcessToy(const Toy& toy, const std::weak_ptr<Playspace>& playspace);

        RenderBatch _currBatch = {};
    };
}