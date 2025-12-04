#include "Tbx/Math/Transform.h"
#include "Tbx/PCH.h"

namespace Tbx
{
    std::string Transform::ToString() const
    {
        return std::format(
            "(Position: {}, Scale: {}, Rotation: {})",
            Position.ToString(),
            Scale.ToString(),
            Rotation.ToString());
    }
}
