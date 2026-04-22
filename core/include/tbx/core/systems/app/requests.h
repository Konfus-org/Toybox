#pragma once
#include "tbx/core/systems/messaging/message.h"
#include "tbx/core/tbx_api.h"

namespace tbx
{
    struct TBX_API ExitApplicationRequest : public Request<void>
    {
    };
}
