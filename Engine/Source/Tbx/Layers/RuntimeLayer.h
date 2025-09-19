#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/App/IRuntime.h"
#include "Tbx/Memory/Refs.h"
#include <memory>
#include <vector>

namespace Tbx
{
    class App;

    /// <summary>
    /// Hosts app runtime modules and coordinates their attachment lifecycle within the layer stack.
    /// </summary>
    class RuntimeLayer : public Layer
    {
    public:
        explicit RuntimeLayer(Tbx::WeakRef<App> app);

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;

        void AddRuntime(const Tbx::Ref<IRuntime>& runtime);
        void RemoveRuntime(const Tbx::Ref<IRuntime>& runtime);
        std::vector<Tbx::Ref<IRuntime>> GetRuntimes() const;

    private:
        void AttachRuntime(const Tbx::Ref<IRuntime>& runtime);
        void DetachRuntime(const Tbx::Ref<IRuntime>& runtime);

        Tbx::WeakRef<App> _app;
        std::vector<Tbx::Ref<IRuntime>> _runtimes;
        bool _isAttached = false;
    };
}

