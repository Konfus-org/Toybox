#include "Tbx/PCH.h"
#include "Tbx/Math/Size.h"
#include <format>

namespace Tbx
{
    std::string Size::ToString() const { return std::format("(Width: {}, Height: {})", Width, Height); }
    float Size::GetAspectRatio() const { return static_cast<float>(Width) / static_cast<float>(Height); }
}
