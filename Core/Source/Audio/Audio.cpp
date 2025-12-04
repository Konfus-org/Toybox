#include "Tbx/Audio/Audio.h"
#include "Tbx/PCH.h"

namespace Tbx
{
    Audio::Audio(SampleData data, AudioFormat format)
        : Data(data)
        , Format(format)
    {
    }
}
