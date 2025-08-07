#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Debug/ILogger.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/Math/Vectors.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    /// Anything in the tbx lib that is intented to be a plugin should inherit from this interface!
    /// Plugins defined inside plugin libraries can also directly inherit from this if they only need to hook into things on load/unload.
    /// Otherwise its recommended to implement one of the TBX interfaces that inherit from this directly (defined below this interface in PluginInterfaces.h).
    /// </summary>
    class EXPORT IPlugin
    {
    public:
        virtual ~IPlugin() = default;

        virtual void OnLoad() = 0;
        virtual void OnUnload() = 0;
    };

    class ILayerPlugin : public IPlugin, public Layer
    {
    public:
        EXPORT ILayerPlugin(const std::string& name) : Layer(name) {}
        EXPORT virtual ~ILayerPlugin() = default;
    };

    template <typename T>
    class EXPORT FactoryPlugin : public IPlugin
    {
    public:
        virtual ~FactoryPlugin() = default;

        /// <summary>
        /// Creates a new instance of the factory item.
        /// The returned shared_ptr will automatically delete the item when it goes out of scope.
        /// </summary>
        std::shared_ptr<T> Create()
        {
            T* newT = New();
            TBX_ASSERT(newT, "Factory failed to create a new item!");
            return std::shared_ptr<T>(newT, [this](T* toDelete) { Delete(toDelete); });
        }

    private:
        virtual T* New() = 0;
        virtual void Delete(T* itemCreatedByFactory) = 0;
    };

    class EXPORT ILoggerFactoryPlugin : public FactoryPlugin<ILogger>
    {
    public:
        virtual ~ILoggerFactoryPlugin() = default;
    };

    class EXPORT IRendererFactoryPlugin : public FactoryPlugin<IRenderer>
    {
    public:
        virtual ~IRendererFactoryPlugin() = default;
    };

    class EXPORT IWindowFactoryPlugin : public FactoryPlugin<IWindow>
    {
    public:
        virtual ~IWindowFactoryPlugin() = default;
    };

    class EXPORT IInputHandlerPlugin : public IPlugin
    {
    public:
        virtual ~IInputHandlerPlugin() = default;

        virtual void SetContext(const std::shared_ptr<IWindow>& windowToListenTo) = 0;

        virtual bool IsGamepadButtonDown(const int gamepadId, const int button) const = 0;
        virtual bool IsGamepadButtonUp(const int gamepadId, const int button) const = 0;
        virtual bool IsGamepadButtonHeld(const int gamepadId, const int button) const = 0;

        virtual bool IsKeyDown(const int keyCode) const = 0;
        virtual bool IsKeyUp(const int keyCode) const = 0;
        virtual bool IsKeyHeld(const int keyCode) const = 0;

        virtual bool IsMouseButtonDown(const int button) const = 0;
        virtual bool IsMouseButtonUp(const int button) const = 0;
        virtual bool IsMouseButtonHeld(const int button) const = 0;
        virtual Vector2 GetMousePosition() const = 0;
    };
}