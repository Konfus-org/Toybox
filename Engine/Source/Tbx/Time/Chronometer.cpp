#include "Tbx/PCH.h"
#include "Tbx/Time/Chronometer.h"

namespace Tbx
{
    Chronometer::Chronometer()
        : _lastSample()
        , _hasLastSample(false)
    {
    }

    void Chronometer::Reset()
    {
        _hasLastSample = false;
    }
}
