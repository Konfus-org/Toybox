#include "tbx/physics/collider.h"

namespace tbx
{
    void ColliderTrigger::request_overlap_scan()
    {
        is_manual_scan_requested = true;
    }
}
