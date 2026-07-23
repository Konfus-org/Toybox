#pragma once
// Codegen-only stub for the math backend seam. The real seam (engine/src/math/<backend>) pulls in glm;
// the codegen only needs the tbx math *type names* to resolve so field types map to the right Luau
// type (Vec3 -> Vec3, not a clang error-recovery int). Given to libclang via -I ahead of the real seam.
// NEVER compiled by the engine.

namespace tbx
{
    struct Vec2 { float x, y; };
    struct Vec3 { float x, y, z; };
    struct Vec4 { float x, y, z, w; };
    struct Quat { float x, y, z, w; };
    struct Mat3 { float m[9]; };
    struct Mat4 { float m[16]; };
}
