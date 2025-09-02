#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Graphics/IRenderer.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Debug/ILogger.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/Math/Vectors.h"
#include <memory>
#include <filesystem>

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

    class EXPORT ILoggerFactoryPlugin : public IPlugin
    {
    public:
        virtual ~ILoggerFactoryPlugin() = default;
        virtual std::shared_ptr<ILogger> Create(const std::string& name, const std::string filePath = "") = 0;
    };

    class EXPORT IRendererFactoryPlugin : public IPlugin
    {
    public:
        virtual ~IRendererFactoryPlugin() = default;
        virtual std::shared_ptr<IRenderer> Create(std::shared_ptr<IRenderSurface> surface) = 0;
    };

    class EXPORT IWindowFactoryPlugin : public IPlugin
    {
    public:
        virtual ~IWindowFactoryPlugin() = default;
        virtual std::shared_ptr<IWindow> Create(const std::string& title, const Size& size, const WindowMode mode) = 0;
    };

    class EXPORT IInputHandlerPlugin : public IPlugin
    {
    public:
        virtual ~IInputHandlerPlugin() = default;

        virtual void Update() = 0;

        virtual bool IsGamepadButtonDown(int playerIndex, int button) const = 0;
        virtual bool IsGamepadButtonUp(int playerIndex, int button) const = 0;
        virtual bool IsGamepadButtonHeld(int playerIndex, int button) const = 0;
        virtual float GetGamepadAxis(int playerIndex, int axis) const = 0;

        virtual bool IsKeyDown(int keyCode) const = 0;
        virtual bool IsKeyUp(int keyCode) const = 0;
        virtual bool IsKeyHeld(int keyCode) const = 0;

        virtual bool IsMouseButtonDown(int button) const = 0;
        virtual bool IsMouseButtonUp(int button) const = 0;
        virtual bool IsMouseButtonHeld(int button) const = 0;
        virtual Vector2 GetMousePosition() const = 0;
    };

    class ITextureLoaderPlugin : public IPlugin
    {
    public:
        virtual std::shared_ptr<Texture> LoadTexture(const std::string& filepath) = 0;
    };

    class IShaderLoaderPlugin : public IPlugin
    {
    public:
        virtual std::shared_ptr<Shader> LoadShader(const std::string& filepath) = 0;
    };
}