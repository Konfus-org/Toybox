#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/app/settings.h"
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
