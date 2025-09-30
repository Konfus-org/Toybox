#include "Tbx/PCH.h"
#include "Tbx/Graphics/Color.h"

namespace Tbx
{
    RgbaColor RgbaColor::White = RgbaColor(1.0f, 1.0f, 1.0f, 1.0f);
    RgbaColor RgbaColor::Black = RgbaColor(0.0f, 0.0f, 0.0f, 1.0f);
    RgbaColor RgbaColor::Red = RgbaColor(1.0f, 0.0f, 0.0f, 1.0f);
    RgbaColor RgbaColor::Green = RgbaColor(0.0f, 1.0f, 0.0f, 1.0f);
    RgbaColor RgbaColor::Blue = RgbaColor(0.0f, 0.0f, 1.0f, 1.0f);
    RgbaColor RgbaColor::Yellow = RgbaColor(1.0f, 1.0f, 0.0f, 1.0f);
    RgbaColor RgbaColor::Cyan = RgbaColor(0.0f, 1.0f, 1.0f, 1.0f);
    RgbaColor RgbaColor::Magenta = RgbaColor(1.0f, 0.0f, 1.0f, 1.0f);
    RgbaColor RgbaColor::Grey = RgbaColor(0.5f, 0.5f, 0.5f, 1.0f);
    RgbaColor RgbaColor::LightGrey = RgbaColor(0.75f, 0.75f, 0.75f, 1.0f);
    RgbaColor RgbaColor::DarkGrey = RgbaColor(0.1f, 0.1f, 0.1f, 1.0f);
}