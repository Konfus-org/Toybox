#pragma once
#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/shader.h"
#include "tbx/types/assets/texture.h"
#include <memory>
#include <vector>

namespace tbx::internal
{
    inline std::shared_ptr<Material> make_fallback_material()
    {
        auto material = Material();
        material.shader.vertex = DefaultPbrVertexShader::HANDLE;
        material.shader.fragment = DefaultPbrFragmentShader::HANDLE;

        material.parameters.set("albedo_color", Color(1.0F, 0.0F, 1.0F, 1.0F));
        material.parameters.set("emissive_color", Color(1.0F, 0.0F, 1.0F, 1.0F));
        material.parameters.set("metallic", 0.0F);
        material.parameters.set("roughness", 1.0F);
        material.parameters.set("normal_strength", 1.0F);
        material.parameters.set("ao", 1.0F);
        material.textures.set("albedo_map", {});
        material.textures.set("normal_map", {});
        material.textures.set("metallic_roughness_map", {});
        material.textures.set("ao_map", {});
        material.textures.set("emissive_map", {});
        material.config = MaterialConfig {
            .is_depth_test_enabled = true,
            .is_depth_write_enabled = true,
            .is_depth_prepass_enabled = false,
            .depth_function = MaterialDepthFunction::LESS,
            .blend_mode = MaterialBlendMode::OPAQUE,
        };
        return std::make_shared<Material>(std::move(material));
    }

    inline std::shared_ptr<ShaderProgram> make_fallback_shader()
    {
        auto vertex_shader = ShaderSource(
            "#version 450 core\n"
            "layout(location = 0) in vec3 a_position;\n"
            "layout(location = 5) in mat4 a_instance_model;\n"
            "layout(std140, binding = 1) uniform TbxCameraData\n"
            "{\n"
            "    mat4 u_view;\n"
            "    mat4 u_projection;\n"
            "    mat4 u_view_projection;\n"
            "    mat4 u_inverse_view;\n"
            "    mat4 u_inverse_projection;\n"
            "    vec4 u_camera_world_position;\n"
            "};\n"
            "layout(std140, binding = 2) uniform TbxObjectData\n"
            "{\n"
            "    mat4 u_model;\n"
            "    mat4 u_normal;\n"
            "};\n"
            "void main()\n"
            "{\n"
            "    gl_Position = u_view_projection * a_instance_model * vec4(a_position, 1.0);\n"
            "}\n",
            ShaderType::VERTEX);

        auto fragment_shader = ShaderSource(
            "#version 450 core\n"
            "layout(location = 0) out vec4 o_color;\n"
            "void main()\n"
            "{\n"
            "    o_color = vec4(1.0, 0.0, 1.0, 1.0);\n"
            "}\n",
            ShaderType::FRAGMENT);

        auto sources = std::vector<ShaderSource>();
        sources.push_back(std::move(vertex_shader));
        sources.push_back(std::move(fragment_shader));
        return std::make_shared<ShaderProgram>(std::move(sources));
    }

    inline std::shared_ptr<Model> make_fallback_model()
    {
        auto material = Material();
        material.textures.set("albedo_map", NotFoundIcon::HANDLE);
        return std::make_shared<Model>(Mesh::CUBE, material);
    }
}
