#include "gl_id_provider.h"

namespace opengl_rendering
{
    tbx::Uuid GlIdProvider::provide(const tbx::Uuid& first, tbx::uint32 second) const
    {
        return tbx::Uuid::combine(first, second);
    }
}
