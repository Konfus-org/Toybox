#pragma once
#include "Tbx/Graphics/GraphicsPipeline.h"

namespace Tbx
{
    // TODO: Here we can add various helpers and ways to hook into the graphics pipeline
    // Perhaps even host multiple pipelines for different things... like opaque passes, screen-space effects, etc.
    class GraphicsManager
    {
    public:
        GraphicsManager() = default;
        GraphicsManager(
            GraphicsApi startingApi,
            const std::vector<Ref<IGraphicsBackend>>& backends,
            const std::vector<Ref<IGraphicsContextProvider>>& contextProviders,
            Ref<EventBus> eventBus) : _pipeline(MakeExclusive<GraphicsPipeline>(startingApi, backends, contextProviders, eventBus)) { }

        void Update() { _pipeline->Update(); }

    private:
        ExclusiveRef<GraphicsPipeline> _pipeline = nullptr;
    };
}
