#pragma once
#include "Tbx/Graphics/GraphicsPipeline.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Events/StageEvents.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Events/RenderEvents.h"
#include <vector>
#include <unordered_map>
#include <utility>

namespace Tbx
{
    class GraphicsManager
    {
    public:
        GraphicsManager() = default;
        GraphicsManager(
            GraphicsApi startingApi,
            const std::vector<Ref<IGraphicsBackend>>& backends,
            const std::vector<Ref<IGraphicsContextProvider>>& contextProviders,
            Ref<EventBus> eventBus);

        void Update();

        void SetRenderPasses(std::vector<RenderPassDescriptor> passes);
        const std::vector<RenderPassDescriptor>& GetRenderPasses() const;

    private:
        void InitializeRenderers(
            const std::vector<Ref<IGraphicsBackend>>& backends,
            const std::vector<Ref<IGraphicsContextProvider>>& contextProviders);
        GraphicsRenderer* GetRenderer(GraphicsApi api);
        void RecreateRenderersForCurrentApi();

        void OnAppSettingsChanged(const AppSettingsChangedEvent& e);
        void OnWindowOpened(const WindowOpenedEvent& e);
        void OnWindowClosed(const WindowClosedEvent& e);
        void OnStageOpened(const StageOpenedEvent& e);
        void OnStageClosed(const StageClosedEvent& e);

        static std::vector<RenderPassDescriptor> CreateDefaultRenderPasses();

    private:
        ExclusiveRef<GraphicsPipeline> _pipeline = nullptr;
        Ref<EventBus> _eventBus = nullptr;
        EventListener _eventListener = {};

        std::vector<Ref<Stage>> _openStages = {};
        std::vector<GraphicsDisplay> _openDisplays = {};
        std::unordered_map<GraphicsApi, GraphicsRenderer> _renderers = {};

        GraphicsApi _activeGraphicsApi = GraphicsApi::None;
        VsyncMode _vsync = VsyncMode::Adaptive;
        RgbaColor _clearColor = {};
    };
}
