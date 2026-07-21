#pragma once

// Builtin assets: always available, no files involved. MeshRenderer takes these mesh names
// directly: sandbox.spawn("Crate").with(MeshRenderer {.mesh = builtin::CUBE}).
namespace tbx::builtin
{
    inline constexpr const char* CUBE = "cube";
    inline constexpr const char* PLANE = "plane";
    inline constexpr const char* SPHERE = "sphere";
}
