#pragma once
#include "opengl_resources/opengl_resource.h"
#include "opengl_resources/opengl_shader.h"
#include "tbx/assets/manager.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include <memory>
#include <type_traits>
#include <unordered_map>

namespace opengl_rendering
{
    class OpenGlResources final
    {
      public:
        OpenGlResources(tbx::AssetManager& asset_manager);

        tbx::Uuid upload_dynamic_mesh(const tbx::DynamicMesh& dynamic_mesh, bool pin = false);
        tbx::Uuid upload_mesh(const tbx::Mesh& mesh, const tbx::Uuid& resource_uuid, bool pin = false);
        tbx::Uuid upload_material(const tbx::MaterialInstance& material, bool pin = false);
        tbx::Uuid upload_material(
            const std::shared_ptr<OpenGlShaderProgram>& shader_program,
            const tbx::Uuid& resource_uuid,
            bool pin = false);
        tbx::Uuid upload_static_mesh(const tbx::StaticMesh& static_mesh, bool pin = false);
        tbx::Uuid upload_texture(const tbx::Handle& texture_handle, bool pin = false);
        tbx::Uuid upload_texture(
            const tbx::Texture& texture,
            const tbx::Uuid& resource_uuid,
            bool pin = false);
        void unload(const tbx::Uuid& resource_uuid);
        std::shared_ptr<tbx::Material> get_material_asset(const tbx::Handle& material_handle);

        template <typename TResource>
        bool try_get(const tbx::Uuid& resource_uuid, std::shared_ptr<TResource>& out_resource) const;

      private:
        bool try_get_raw(
            const tbx::Uuid& resource_uuid,
            std::shared_ptr<IOpenGlResource>& out_resource) const;

      private:
        tbx::AssetManager& _asset_manager;
        std::unordered_map<tbx::Uuid, std::shared_ptr<IOpenGlResource>> _resources = {};
    };
}

#include "opengl_resources.inl"
