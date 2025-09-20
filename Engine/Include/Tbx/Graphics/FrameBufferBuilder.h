#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/TSS/Toy.h"
#include "Tbx/TSS/Views.h"
#include "Tbx/Graphics/Material.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Graphics/Model.h"
#include "Tbx/Graphics/Buffers.h"
#include "Tbx/Memory/Refs.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    /// Builds frame buffers of render commands from boxes and their toys.
    /// </summary>
    class EXPORT FrameBufferBuilder
    {
    public:
        /// <summary>
        /// Generates commands to upload resources for the provided boxes.
        /// </summary>
        FrameBuffer BuildUploadBuffer(StageView<MaterialInstance, Mesh, Model> view);

        /// <summary>
        /// Generates commands necessary to render the provided boxes.
        /// </summary>
        FrameBuffer BuildRenderBuffer(FullStageViewView view);

    private:
        void AddToyUploadCommandsToBuffer(const Tbx::Ref<Toy>& toy, FrameBuffer& buffer);
        void AddToyRenderCommandsToBuffer(const Tbx::Ref<Toy>& toy, FrameBuffer& buffer);
    };
}
