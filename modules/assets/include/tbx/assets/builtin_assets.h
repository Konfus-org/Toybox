#pragma once
#include "tbx/common/handle.h"

namespace tbx
{
    inline const Handle default_shader = Handle(Uuid(0x1U)); // Used when no shader is specified.
    inline const Handle default_material =
        Handle(Uuid(0x2U)); // Used when no material is specified.
    inline const Handle not_found_texture = Handle(Uuid(0x3U)); // Used when an asset is not found.
}
