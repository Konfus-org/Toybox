#include "gl_id_provider.h"

namespace opengl_rendering
{
    using namespace tbx;
    Uuid GlIdProvider::provide(const Uuid& first, uint32 second) const
    {
        return Uuid::combine(first, second);
    }
}
