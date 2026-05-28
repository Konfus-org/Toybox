#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/app/settings.h"

namespace tbx
{
    AppSettings::AppSettings(
        std::weak_ptr<IMessageDispatcher> dispatcher,
        bool vsync,
        GraphicsApi api,
        Size resolution,
        AsyncSettings async_settings)
        : graphics(dispatcher, *this, &AppSettings::graphics, std::in_place, vsync, api, resolution)
        , physics(dispatcher)
        , async(std::move(async_settings))
    {
    }
}
