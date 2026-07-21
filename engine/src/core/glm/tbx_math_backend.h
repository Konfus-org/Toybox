#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// The glm math backend's TYPE surface — the tbx::math functions are declared in
// tbx/core/math.h and implemented by this backend's math_glm.cpp.
namespace tbx
{
    using Vec2 = glm::vec2;
    using Vec3 = glm::vec3;
    using Vec4 = glm::vec4;
    using Quat = glm::quat;
    using Mat4 = glm::mat4;
}
