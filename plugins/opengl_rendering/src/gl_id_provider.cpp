#include "gl_id_provider.h"

namespace opengl_rendering
{
    Uuid GlIdProvider::provide(const Uuid& first, uint32 second) const
    {
        return Uuid::combine(first, second);
    }
}
