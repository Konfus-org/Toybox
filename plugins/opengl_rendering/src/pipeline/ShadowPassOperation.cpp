#include "ShadowPassOperation.h"

namespace opengl_rendering
{
    ShadowPassOperation::ShadowPassOperation(OpenGlResourceManager&)
    {
    }

    ShadowPassOperation::~ShadowPassOperation() noexcept = default;

    void ShadowPassOperation::execute(const std::any&)
    {
    }

    tbx::uint32 ShadowPassOperation::get_shadow_texture() const
    {
        return 0U;
    }
}
