#include "Tbx/PCH.h"
#include "Tbx/Time/Chronometer.h"

namespace Tbx
{
    Chronometer::Chronometer()
        : _lastSample()
        , _hasLastSample(false)
    {
    }

    Chronometer::Seconds Chronometer::Tick()
    {
        const auto now = Clock::now();

        if (!_hasLastSample)
        {
            _hasLastSample = true;
            _lastSample = now;
            return Seconds::zero();
        }

        const auto elapsed = now - _lastSample;
        _lastSample = now;
        return std::chrono::duration_cast<Seconds>(elapsed);
    }

    void Chronometer::Reset()
    {
        _hasLastSample = false;
    }
}
