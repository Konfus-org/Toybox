#include "tbx/graphics/color.h"

namespace tbx
{
    RgbaColor RgbaColor::white = RgbaColor(1.0f, 1.0f, 1.0f, 1.0f);
    RgbaColor RgbaColor::black = RgbaColor(0.0f, 0.0f, 0.0f, 1.0f);
    RgbaColor RgbaColor::red = RgbaColor(1.0f, 0.0f, 0.0f, 1.0f);
    RgbaColor RgbaColor::green = RgbaColor(0.0f, 1.0f, 0.0f, 1.0f);
    RgbaColor RgbaColor::blue = RgbaColor(0.0f, 0.0f, 1.0f, 1.0f);
    RgbaColor RgbaColor::yellow = RgbaColor(1.0f, 1.0f, 0.0f, 1.0f);
    RgbaColor RgbaColor::cyan = RgbaColor(0.0f, 1.0f, 1.0f, 1.0f);
    RgbaColor RgbaColor::magenta = RgbaColor(1.0f, 0.0f, 1.0f, 1.0f);
    RgbaColor RgbaColor::grey = RgbaColor(0.5f, 0.5f, 0.5f, 1.0f);
    RgbaColor RgbaColor::light_grey = RgbaColor(0.75f, 0.75f, 0.75f, 1.0f);
    RgbaColor RgbaColor::dark_grey = RgbaColor(0.1f, 0.1f, 0.1f, 1.0f);
}
