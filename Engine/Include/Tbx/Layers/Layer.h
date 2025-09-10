#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Layers/LayerStack.h"
#include <string>
#include <memory>

namespace Tbx
{
    // Forward declaration to avoid including App.h; layers only store a weak reference.
    class App;

    /// <summary>
    /// An application layer. Used to cleanly add and seperate functionality.
    /// Some examples are a graphics layer, windowing layer, input layer, etc...
    /// </summary>
    class Layer
    {
    public:
        EXPORT explicit(false) Layer(const std::string& name);
        EXPORT virtual ~Layer();

        EXPORT void Update();

        EXPORT virtual void OnAttach() {}
        EXPORT virtual void OnDetach() {}
        EXPORT virtual void OnUpdate() {}

        EXPORT std::string GetName() const;

        EXPORT void SetApp(const std::shared_ptr<App>& app) { _app = app; }

    protected:
        template <typename T, typename... Args>
        std::shared_ptr<T> EmplaceLayer(Args&&... args)
        {
            auto layer = std::make_shared<T>(std::forward<Args>(args)...);
            layer->SetApp(_app.lock());
            _subLayers.Push(layer);
            return layer;
        }

        LayerStack _subLayers;
        std::weak_ptr<App> _app;

    private:
        std::string _name;
    };
}