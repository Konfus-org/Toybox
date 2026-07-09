#include "gizmo_op_replay.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/vectors.h"
#include <glm/gtc/type_ptr.hpp>
#include <string>

namespace tbx::studio_bridge
{
    //// WIRE PARSING ////

    // Reads are lenient, mirroring the editor's WireValue codecs: a missing or malformed token yields
    // the fallback rather than throwing, so a misbehaving peer can never crash the replay.
    static float read_float(const tbx::Json& token, float fallback = 0.0F)
    {
        return token.is_number() ? token.get<float>() : fallback;
    }

    static float read_arg_float(const tbx::Json& op, size index, float fallback = 0.0F)
    {
        return index < op.size() ? read_float(op[index], fallback) : fallback;
    }

    static tbx::Vec2 read_arg_vec2(const tbx::Json& op, size index)
    {
        if (index >= op.size() || !op[index].is_array() || op[index].size() < 2)
            return tbx::Vec2(0.0F);
        const auto& token = op[index];
        return {read_float(token[0]), read_float(token[1])};
    }

    static tbx::Vec3 read_arg_vec3(const tbx::Json& op, size index)
    {
        if (index >= op.size() || !op[index].is_array() || op[index].size() < 3)
            return tbx::Vec3(0.0F);
        const auto& token = op[index];
        return {read_float(token[0]), read_float(token[1]), read_float(token[2])};
    }

    // The wire spells quaternions [x, y, z, w]; a missing trailing rotation argument is the engine
    // default (identity), matching the editor's optional rotation parameters.
    static tbx::Quat read_arg_quat(const tbx::Json& op, size index)
    {
        if (index >= op.size() || !op[index].is_array() || op[index].size() < 4)
            return tbx::Quat(1.0F, 0.0F, 0.0F, 0.0F);
        const auto& token = op[index];
        return {read_float(token[3], 1.0F), read_float(token[0]), read_float(token[1]), read_float(token[2])};
    }

    // Colors ride as {r,g,b,a} normalized floats (the editor's WireValue color shape).
    static tbx::Color read_arg_color(const tbx::Json& op, size index)
    {
        if (index >= op.size() || !op[index].is_object())
            return tbx::Color(1.0F, 1.0F, 1.0F, 1.0F);
        const auto& token = op[index];
        return {
            token.value("r", 1.0F), token.value("g", 1.0F), token.value("b", 1.0F), token.value("a", 1.0F)};
    }

    // The wire sends matrices row-major (System.Numerics order); glm is column-major, so transpose.
    static tbx::Mat4 read_arg_mat4(const tbx::Json& op, size index)
    {
        if (index >= op.size() || !op[index].is_array() || op[index].size() < 16)
            return tbx::Mat4(1.0F);
        const auto& token = op[index];
        float values[16];
        for (size i = 0; i < 16; ++i)
            values[i] = read_float(token[i]);
        return glm::transpose(glm::make_mat4(values));
    }

    //// OP REPLAY ////

    // Replays one drawing op into the batch. `color` threads the current draw color to the primitives
    // whose engine signatures take it explicitly rather than from set_color state; `base_matrix` and
    // `color_override` implement the caller-space / highlight semantics documented on replay_gizmo_ops.
    static void replay_op(
        tbx::Gizmos& gizmos,
        const tbx::Json& op,
        tbx::Color& color,
        const tbx::Mat4* base_matrix,
        bool color_locked)
    {
        if (!op.is_array() || op.empty() || !op[0].is_string())
            return;

        const auto name = op[0].get<std::string>();
        if (name == "color")
        {
            if (color_locked)
                return;
            color = read_arg_color(op, 1);
            gizmos.set_color(color);
        }
        else if (name == "matrix")
        {
            const auto matrix = read_arg_mat4(op, 1);
            gizmos.set_matrix(base_matrix != nullptr ? (*base_matrix * matrix) : matrix);
        }
        else if (name == "reset_matrix")
        {
            if (base_matrix != nullptr)
                gizmos.set_matrix(*base_matrix);
            else
                gizmos.reset_matrix();
        }
        else if (name == "line")
            gizmos.line(read_arg_vec3(op, 1), read_arg_vec3(op, 2));
        else if (name == "ray")
            gizmos.ray(read_arg_vec3(op, 1), read_arg_vec3(op, 2), read_arg_float(op, 3));
        else if (name == "wire_box")
            gizmos.wire_box(read_arg_vec3(op, 1), read_arg_vec3(op, 2), read_arg_quat(op, 3));
        else if (name == "wire_sphere")
            gizmos.wire_sphere(read_arg_vec3(op, 1), read_arg_float(op, 2));
        else if (name == "wire_capsule")
            gizmos.wire_capsule(
                read_arg_vec3(op, 1), read_arg_float(op, 2), read_arg_float(op, 3), read_arg_quat(op, 4));
        else if (name == "ring")
            gizmos.ring(read_arg_vec3(op, 1), read_arg_vec3(op, 2), read_arg_float(op, 3));
        else if (name == "arrow")
            gizmos.arrow(read_arg_vec3(op, 1), read_arg_vec3(op, 2));
        else if (name == "solid_arrow")
            gizmos.solid_arrow(read_arg_vec3(op, 1), read_arg_vec3(op, 2), color, read_arg_float(op, 3));
        else if (name == "solid_beam")
            gizmos.solid_beam(read_arg_vec3(op, 1), read_arg_vec3(op, 2), color, read_arg_float(op, 3));
        else if (name == "solid_torus")
            gizmos.solid_torus(
                read_arg_vec3(op, 1), read_arg_vec3(op, 2), read_arg_float(op, 3), read_arg_float(op, 4), color);
        else if (name == "filled_arc")
            gizmos.filled_arc(
                read_arg_vec3(op, 1),
                read_arg_vec3(op, 2),
                read_arg_float(op, 3),
                read_arg_vec3(op, 4),
                read_arg_float(op, 5),
                color);
        else if (name == "wire_square")
            gizmos.wire_square(read_arg_vec3(op, 1), read_arg_vec2(op, 2), read_arg_quat(op, 3));
        else if (name == "wire_plane")
            gizmos.wire_plane(read_arg_vec3(op, 1), read_arg_vec3(op, 2), read_arg_float(op, 3));
        else if (name == "axes")
            gizmos.axes(read_arg_vec3(op, 1), read_arg_quat(op, 2), read_arg_float(op, 3));
        else if (name == "solid_box")
            gizmos.solid_box(read_arg_vec3(op, 1), read_arg_vec3(op, 2), read_arg_quat(op, 3));
        else if (name == "solid_sphere")
            gizmos.solid_sphere(read_arg_vec3(op, 1), read_arg_float(op, 2));
        else if (name == "solid_square")
            gizmos.solid_square(read_arg_vec3(op, 1), read_arg_vec2(op, 2), read_arg_quat(op, 3));
        else if (name == "solid_plane")
            gizmos.solid_plane(read_arg_vec3(op, 1), read_arg_vec3(op, 2), read_arg_float(op, 3));
        else
            TBX_TRACE_WARNING_ONCE("StudioBridge: unknown gizmo op '{}' skipped.", name);
    }

    void replay_gizmo_ops(
        tbx::Gizmos& gizmos,
        const tbx::Json& ops,
        const tbx::Mat4* base_matrix,
        const tbx::Color* color_override)
    {
        // Every stream replays from the same defaults the engine's immediate mode starts with, so one
        // stream's trailing color/matrix state never leaks into the next.
        auto color = color_override != nullptr ? *color_override : tbx::Color(1.0F, 1.0F, 1.0F, 1.0F);
        gizmos.set_color(color);
        if (base_matrix != nullptr)
            gizmos.set_matrix(*base_matrix);
        else
            gizmos.reset_matrix();

        if (ops.is_array())
            for (const auto& op : ops)
                replay_op(gizmos, op, color, base_matrix, color_override != nullptr);

        gizmos.set_color(tbx::Color(1.0F, 1.0F, 1.0F, 1.0F));
        gizmos.reset_matrix();
    }
}
