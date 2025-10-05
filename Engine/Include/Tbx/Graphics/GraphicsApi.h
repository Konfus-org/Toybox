#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Viewport.h"

namespace Tbx
{
    class IGraphicsContext;

    enum class TBX_EXPORT GraphicsApi
    {
        None,
        Vulkan,
        OpenGL,
        Metal,
        Custom
    };

    /// <summary>
    /// Says something uses graphics apis.
    /// Is resposible for providing what Apis it supports.
    /// </summary>
    class TBX_EXPORT IUseGraphicsApis
    {
    public:
        virtual ~IUseGraphicsApis() = default;
        virtual std::vector<GraphicsApi> GetSupportedApis() const = 0;
    };
 
    /// <summary>
    /// Says something is responsible for a graphics api.
    /// It handles initialization and shutdown of the api.
    /// </summary>
    class TBX_EXPORT IManageGraphicsApis : public IUseGraphicsApis
    {
    public:
        virtual ~IManageGraphicsApis() = default;
        virtual void Initialize(Ref<IGraphicsContext> context, GraphicsApi api) = 0;
        virtual void Shutdown() = 0;

    };
}