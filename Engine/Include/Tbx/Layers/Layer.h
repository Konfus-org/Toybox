#pragma once
#include "Tbx/DllExport.h"
#include <string>
#include <memory>

namespace Tbx
{
    class App;

    /// <summary>
    /// An application layer. Used to cleanly add and seperate functionality.
    /// Some examples are a graphics layer, windowing layer, input layer, etc...
    /// </summary>
    class Layer : std::enable_shared_from_this<Layer>
    {
    public:
        EXPORT explicit(false) Layer(const std::string& name, const std::weak_ptr<App>& app)
            : _name(name), _app(app) {}
        EXPORT ~Layer();

        EXPORT std::string GetName() const;
        EXPORT std::shared_ptr<App> GetApp() const;

        EXPORT void AttachTo(std::vector<std::shared_ptr<Layer>>& layers);
        EXPORT void DetachFrom(std::vector<std::shared_ptr<Layer>>& layers);

        EXPORT void Update();

    protected:
        EXPORT virtual void OnAttach() {}
        EXPORT virtual void OnDetach() {}
        EXPORT virtual void OnUpdate() {}

    private:
        std::weak_ptr<App> _app = {};
        std::string _name = "";
    };
}