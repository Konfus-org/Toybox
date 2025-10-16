#pragma once
#include "Tbx/Graphics/GraphicsPipeline.h"
#include "Tbx/Graphics/RenderPass.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventCarrier.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Events/StageEvents.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Events/AppEvents.h"
#include <unordered_map>
#include <vector>

namespace Tbx
{
    class TBX_EXPORT IGraphicsManager
    {
    public:
        virtual ~IGraphicsManager() = default;
        virtual void Update() = 0;
        virtual void SetRenderPasses(const std::vector<RenderPass>& passes) = 0;
        virtual const std::vector<RenderPass>& GetRenderPasses() const = 0;
    };

    class TBX_EXPORT HeadlessGraphicsManager final : public IGraphicsManager
    {
    public:
        HeadlessGraphicsManager() = default;
        void Update() override {}
        void SetRenderPasses(const std::vector<RenderPass>& passes) override {}
        const std::vector<RenderPass>& GetRenderPasses() const override
        {
            static std::vector<RenderPass> empty = {};
            return empty;
        }
    };

    class TBX_EXPORT GraphicsManager final : public IGraphicsManager
    {
    public:
        GraphicsManager() = default;
        GraphicsManager(
            GraphicsApi startingApi,
            const std::vector<Ref<IGraphicsBackend>>& backends,
            const std::vector<Ref<IGraphicsContextProvider>>& contextProviders,
            Ref<EventBus> eventBus);

        void Update() override;

        void SetRenderPasses(const std::vector<RenderPass>& passes) override;
        const std::vector<RenderPass>& GetRenderPasses() const override;

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

        static std::vector<RenderPass> CreateDefaultRenderPasses();

    private:
        GraphicsPipeline _pipeline = {};

        EventListener _eventListener = {};
        EventCarrier _carrier = {};

        std::unordered_map<GraphicsApi, GraphicsRenderer> _renderers = {};
        std::vector<GraphicsDisplay> _openDisplays = {};
        std::vector<const Stage*> _openStages = {};

        GraphicsApi _activeGraphicsApi = GraphicsApi::None;
        VsyncMode _vsync = VsyncMode::Adaptive;
        RgbaColor _clearColor = {};
    };
}
