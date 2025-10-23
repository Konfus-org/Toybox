#include "Tbx/PCH.h"
#include "Tbx/Audio/Audio.h"

namespace Tbx
{
    Audio::Audio(SampleData data, AudioFormat format)
        : Data(data)
        , Format(format)
    {
    }
}
