#include "Tbx/PCH.h"
#include "Tbx/Layers/RuntimeLayer.h"
#include "Tbx/App/App.h"

namespace Tbx
{
    RuntimeLayer::RuntimeLayer(WeakRef<App> app)
        : Layer("Runtime"), _app(std::move(app))
    {
    }

    void RuntimeLayer::OnAttach()
    {
        _isAttached = true;
        if (_app.expired())
        {
            return;
        }

        for (const auto& runtime : _runtimes)
        {
            AttachRuntime(runtime);
        }
    }

    void RuntimeLayer::OnDetach()
    {
        for (const auto& runtime : std::ranges::reverse_view(_runtimes))
        {
            DetachRuntime(runtime);
        }
        _isAttached = false;
    }

    void RuntimeLayer::OnUpdate()
    {
        if (!_isAttached)
        {
            return;
        }

        if (_app.expired())
        {
            return;
        }

        for (const auto& runtime : _runtimes)
        {
            runtime->OnUpdate(_app);
        }
    }

    void RuntimeLayer::AddRuntime(Ref<IRuntime> runtime)
    {
        if (!runtime)
        {
            return;
        }

        if (std::find(_runtimes.begin(), _runtimes.end(), runtime) != _runtimes.end())
        {
            return;
        }

        _runtimes.push_back(runtime);

        if (_isAttached)
        {
            AttachRuntime(runtime);
        }
    }

    void RuntimeLayer::RemoveRuntime(Ref<IRuntime> runtime)
    {
        if (!runtime)
        {
            return;
        }

        auto it = std::find(_runtimes.begin(), _runtimes.end(), runtime);
        if (it == _runtimes.end())
        {
            return;
        }

        if (_isAttached)
        {
            DetachRuntime(runtime);
        }

        _runtimes.erase(it);
    }

    std::vector<Ref<IRuntime>> RuntimeLayer::GetRuntimes() const
    {
        return _runtimes;
    }

    void RuntimeLayer::AttachRuntime(Ref<IRuntime> runtime)
    {
        if (!runtime || _app.expired())
        {
            return;
        }

        runtime->OnAttach(_app);
    }

    void RuntimeLayer::DetachRuntime(Ref<IRuntime> runtime)
    {
        if (!runtime)
        {
            return;
        }

        runtime->OnDetach(_app);
    }
}

