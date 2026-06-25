#include "tbx/types/components/renderer.h"

namespace tbx
{
    Renderer::Renderer(Handle model_handle)
        : model(std::move(model_handle))
    {
    }
}
