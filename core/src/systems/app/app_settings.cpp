#include "tbx/core/systems/app/settings.h"
#include "tbx/core/interfaces/message_dispatcher.h"
#include <utility>

namespace tbx
{
    AppSettings::AppSettings(
        IMessageDispatcher& dispatcher,
        bool vsync,
        GraphicsApi api,
        Size resolution,
        AsyncSettings async_settings)
        : graphics(dispatcher, vsync, api, resolution)
        , physics(dispatcher)
        , async(std::move(async_settings))
    {
    }
}
