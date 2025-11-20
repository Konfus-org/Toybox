#include "tbx/math/transform.h"
#include <format>

namespace Tbx
{
    std::string to_string() const
    {
        return std::format("(Position: {}, Scale: {}, Rotation: {})", Position.ToString(), Scale.ToString(), Rotation.ToString());
    }
}
