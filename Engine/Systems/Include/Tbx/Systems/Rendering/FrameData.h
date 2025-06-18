#pragma once
#include "Tbx/Utils/DllExport.h";
#include "Tbx/Utils/Time/DeltaTime.h";
#include "Tbx/Systems/Rendering/IRenderer.h";
#include <any>

namespace Tbx
{
    struct FrameData
    {
    public:
        FrameData(std::any _commandBuffer) 
            : _commandBuffer(_commandBuffer) {}

        /// <summary>
        /// The native command buffer.
        /// </summary>
        std::any GetCommandBuffer() { return _commandBuffer; }

    private:
        std::any _commandBuffer;
    };
}
