#pragma once
#include "Tbx/Utils/DllExport.h"
#include "Tbx/Systems/Rendering/RenderProcessor.h"

namespace Tbx
{
    class EXPORT RenderPipeline
    {
    public:
        /// <summary>
        /// Pre processes a playspace and gets it ready for rendering.
        /// </summary>
        void GetPlayspaceReadyForRendering(std::shared_ptr<Playspace> playspace);

        /// <summary>
        /// Renders a playspace.
        /// </summary>
        void RenderPlayspace(std::shared_ptr<Playspace> playspace);
    };
}
