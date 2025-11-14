#pragma once

#include <glm/glm.hpp>

namespace tbx
{
    /**
     * Provides direct access to the GLM namespace configured with Toybox defaults.
     *
     * The alias exposes every GLM type and function without copying or owning any
     * resources, so consumers can freely include this header wherever mathematical
     * helpers are needed.
     *
     * Thread Safety: The alias is inherently thread-safe because it introduces no
     * state and simply refers to the underlying GLM definitions.
     */
    namespace math = glm;
}
