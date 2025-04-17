#pragma once
#include "Tbx/Runtime/Render Pipeline/RenderQueue.h"
#include "Tbx/Runtime/Render Pipeline/RenderProcessor.h"
#include "Tbx/Runtime/Layers/Layer.h"
#include <Tbx/Core/Rendering/RenderData.h>
#include "Tbx/Core/TBS/Playspace.h"
#include <Tbx/Core/Ids/UID.h>

namespace Tbx
{
    class RenderPipeline : public Layer
    {
    public:
        ~RenderPipeline() override = default;

        void SetContext(const std::shared_ptr<Playspace>& playspace);

        // TODO: extract render settings class
        void SetVSyncEnabled(bool enabled);
        bool IsVSyncEnabled() const;

        void Clear() const;
        void Flush();

        bool IsOverlay() override;
        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;

    private:
        void ProcessNextBatch();

        std::shared_ptr<Playspace> _currentPlayspace;
        RenderProcessor _renderProcessor;
        RenderQueue _renderQueue = {};

        bool _vsyncEnabled;
    };
}
