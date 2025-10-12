#include "Tbx/PCH.h"
#include "Tbx/Time/Chronometer.h"

namespace Tbx
{
    Chronometer::Chronometer()
        : _lastSample()
        , _systemTime()
        , _deltaTime(Seconds::zero())
        , _accumulatedTime(Seconds::zero())
        , _hasLastSample(false)
    {
    }

    void Chronometer::Tick()
    {
        const auto now = Clock::now();
        _systemTime = SystemClock::now();

        if (!_hasLastSample)
        {
            _hasLastSample = true;
            _lastSample = now;
            _deltaTime = Seconds::zero();
            return;
        }

        const auto elapsed = now - _lastSample;
        _lastSample = now;

        _deltaTime = std::chrono::duration_cast<Seconds>(elapsed);
        _accumulatedTime += _deltaTime;
    }

    void Chronometer::Reset()
    {
        _hasLastSample = false;
        _deltaTime = Seconds::zero();
        _accumulatedTime = Seconds::zero();
        _systemTime = SystemClock::now();
        _lastSample = Clock::time_point{};
    }

    DeltaTime Chronometer::GetDeltaTime() const
    {
        return DeltaTime(_deltaTime.count());
    }

    Chronometer::Seconds Chronometer::GetAccumulatedTime() const
    {
        return _accumulatedTime;
    }

    Chronometer::SystemClock::time_point Chronometer::GetSystemTime() const
    {
        return _systemTime;
    }
}
