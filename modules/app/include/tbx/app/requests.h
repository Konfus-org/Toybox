#pragma once
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    struct TBX_API ExitApplicationRequest : public Request<void>
    {
    };
}
