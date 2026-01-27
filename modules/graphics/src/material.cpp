#include "tbx/graphics/material.h"

namespace tbx
{
    const ShaderProgram& get_standard_shader_program()
    {
        static const ShaderProgram program = ShaderProgram();
        return program;
    }
}
