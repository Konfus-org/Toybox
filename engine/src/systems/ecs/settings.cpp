#include "tbx/systems/ecs/settings.h"

namespace tbx
{
    WorldSettings::WorldSettings()
        : chunk_size(*this, &WorldSettings::chunk_size, 32.0F)
        , unload_radius_chunks(*this, &WorldSettings::unload_radius_chunks, 6U)
    {
    }

    WorldSettings::WorldSettings(std::weak_ptr<IMessageDispatcher> dispatcher)
        : chunk_size(dispatcher, *this, &WorldSettings::chunk_size, 32.0F)
        , unload_radius_chunks(dispatcher, *this, &WorldSettings::unload_radius_chunks, 6U)
    {
    }
}
