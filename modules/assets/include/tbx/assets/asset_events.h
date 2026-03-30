#pragma once
#include "tbx/common/handle.h"
#include "tbx/messages/message.h"

namespace tbx
{
    struct AssetReloadedEvent : Event
    {
        Handle affected_asset;
    };
}