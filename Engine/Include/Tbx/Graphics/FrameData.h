#pragma once
#include "Tbx/DllExport.h";
#include "Tbx/Time/DeltaTime.h";
#include "Tbx/Graphics/IRenderer.h";
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
