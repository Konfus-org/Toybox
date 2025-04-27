#pragma once
#include "Tbx/Runtime/Render Pipeline/RenderProcessor.h"
#include "Tbx/Runtime/Layers/Layer.h"
#include <Tbx/Core/Rendering/RenderQueue.h>
#include <Tbx/Core/Rendering/RenderData.h>
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/Core/Events/WorldEvents.h>
#include "Tbx/Core/TBS/Playspace.h"
#include <Tbx/Core/Ids/UID.h>

namespace Tbx
{
    class RenderPipeline : public Layer
    {
    public:
        RenderPipeline(const std::string_view& name) : Layer(name) {}
        ~RenderPipeline() override = default;

        bool IsOverlay() override;
        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;

    private:
        void OnMainPlayspaceChangedEvent(const WorldMainPlayspaceChangedEvent& e);
        void Clear() const;
        void Flush();
        void ProcessNextBatch();

        UID _worldMainPlayspaceChangedEventId = -1;
        RenderProcessor _renderProcessor = {};
        RenderQueue _renderQueue = {};
    };
}
