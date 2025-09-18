#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/App/IRuntime.h"
#include <memory>
#include <vector>

namespace Tbx
{
    class App;

    class RuntimeLayer : public Layer
    {
    public:
        explicit RuntimeLayer(std::weak_ptr<App> app);

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;

        void AddRuntime(const std::shared_ptr<IRuntime>& runtime);
        void RemoveRuntime(const std::shared_ptr<IRuntime>& runtime);
        std::vector<std::shared_ptr<IRuntime>> GetRuntimes() const;

    private:
        void AttachRuntime(const std::shared_ptr<IRuntime>& runtime);
        void DetachRuntime(const std::shared_ptr<IRuntime>& runtime);

        std::weak_ptr<App> _app;
        std::vector<std::shared_ptr<IRuntime>> _runtimes;
        bool _isAttached = false;
    };
}

