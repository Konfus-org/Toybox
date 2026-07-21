#include "tbx/debug/log.h"
#include "tbx/gfx/gpu.h"
#include <cstring>

namespace tbx::gpu
{
    //// HELPERS ////

    static bool read_floats(const serialization::Json& value, float* out, const int count)
    {
        if (!value.is_array() || static_cast<int>(value.size()) < count)
            return false;
        for (int i = 0; i < count; ++i)
        {
            if (!value.at(i).is_number())
                return false;
            out[i] = value.at(i).get<float>();
        }
        return true;
    }

    //// APPLY (backend-agnostic: reflection types the values, set_uniform does the work) ////

    void apply_uniforms(const Shader& shader, const serialization::Json& values)
    {
        if (!values.is_object())
            return;
        const ShaderInfo info = reflect(shader);
        for (const UniformInfo& uniform : info.uniforms)
        {
            const auto it = values.find(uniform.name);
            if (it == values.end())
                continue; // the shader keeps its current/default value
            const serialization::Json& value = *it;
            const char* name = uniform.name.c_str();
            float floats[16] = {};
            bool applied = true;
            switch (uniform.kind)
            {
                case UniformKind::FLOAT:
                    applied = value.is_number();
                    if (applied)
                        set_uniform(shader, name, value.get<float>());
                    break;
                case UniformKind::INT:
                case UniformKind::TEXTURE:
                    applied = value.is_number_integer();
                    if (applied)
                        set_uniform(shader, name, value.get<int>());
                    break;
                case UniformKind::BOOL:
                    applied = value.is_boolean();
                    if (applied)
                        set_uniform(shader, name, value.get<bool>() ? 1 : 0);
                    break;
                case UniformKind::VEC2:
                    applied = read_floats(value, floats, 2);
                    if (applied)
                        set_uniform(shader, name, Vec2(floats[0], floats[1]));
                    break;
                case UniformKind::VEC3:
                    applied = read_floats(value, floats, 3);
                    if (applied)
                        set_uniform(shader, name, Vec3(floats[0], floats[1], floats[2]));
                    break;
                case UniformKind::VEC4:
                    applied = read_floats(value, floats, 4);
                    if (applied)
                        set_uniform(
                            shader, name, Vec4(floats[0], floats[1], floats[2], floats[3]));
                    break;
                case UniformKind::MAT4:
                {
                    applied = read_floats(value, floats, 16);
                    if (applied)
                    {
                        auto matrix = Mat4(1.0f);
                        std::memcpy(&matrix[0][0], floats, sizeof(floats));
                        set_uniform(shader, name, matrix);
                    }
                    break;
                }
                case UniformKind::UNKNOWN:
                    applied = false;
                    break;
            }
            if (!applied)
                TBX_WARN("uniform '{}': value shape does not match the shader", uniform.name);
        }
    }
}
