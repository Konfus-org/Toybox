#pragma once
#include "opengl_resources/opengl_shader.h"

namespace opengl_rendering
{
    class OpenGlUploader;

    OpenGlMaterialParams create_magenta_fallback_material_params(
        const tbx::Handle& material_handle);
    const tbx::Uuid& get_fallback_texture_resource_id();
    tbx::Uuid get_fallback_texture(OpenGlUploader& resource_manager);
    const tbx::Uuid& get_flat_normal_texture_resource_id();
    tbx::Uuid get_flat_normal_texture(OpenGlUploader& resource_manager);
    const tbx::Uuid& get_fallback_material_resource_id();
    tbx::Uuid get_fallback_material(OpenGlUploader& resource_manager);
}
