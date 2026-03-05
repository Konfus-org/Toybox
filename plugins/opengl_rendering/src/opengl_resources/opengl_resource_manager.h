#pragma once
#include "opengl_resource.h"
#include "opengl_shader.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include <chrono>
#include <memory>
#include <type_traits>
#include <unordered_map>

namespace opengl_rendering
{
    class OpenGlResourceManager final
    {
      public:
        OpenGlResourceManager(tbx::AssetManager& asset_manager);

        tbx::Uuid add_dynamic_mesh(const tbx::DynamicMesh& dynamic_mesh, bool pin = false);
        tbx::Uuid add_static_mesh(const tbx::StaticMesh& static_mesh, bool pin = false);
        tbx::Uuid add_material(const tbx::MaterialInstance& material, bool pin = false);
        tbx::Uuid add_material(
            const std::shared_ptr<OpenGlShaderProgram>& shader_program,
            const tbx::Uuid& resource_uuid,
            bool pin = false);
        tbx::Uuid add_texture(
            const tbx::Texture& texture,
            const tbx::Uuid& resource_uuid,
            bool pin = false);
        tbx::Uuid add_texture(const tbx::Handle& texture_handle, bool pin = false);

        template <typename TResource>
        bool try_get(const tbx::Uuid& resource_uuid, std::shared_ptr<TResource>& out_resource) const
        {
            static_assert(
                std::is_base_of_v<IOpenGlResource, TResource>,
                "OpenGlResourceManager::try_get<TResource> requires TResource to derive "
                "from IOpenGlResource.");

            auto out_untyped_resource = std::shared_ptr<IOpenGlResource> {};
            if (!try_get_raw(resource_uuid, out_untyped_resource))
                return false;

            auto out_typed_resource = std::dynamic_pointer_cast<TResource>(out_untyped_resource);
            if (!out_typed_resource)
                return false;

            out_resource = std::move(out_typed_resource);
            return true;
        }

        void pin(const tbx::Uuid& resource_uuid);
        void unpin(const tbx::Uuid& resource_uuid);

        void clear();
        void clear_unused();

      private:
        bool try_get_raw(
            const tbx::Uuid& resource_uuid,
            std::shared_ptr<IOpenGlResource>& out_resource) const;

      private:
        tbx::AssetManager& _asset_manager;
        std::unordered_map<tbx::Uuid, std::shared_ptr<IOpenGlResource>> _resources = {};
        std::unordered_map<tbx::Uuid, std::shared_ptr<IOpenGlResource>> _pinned_resources = {};
        std::unordered_map<tbx::Uuid, std::chrono::steady_clock::time_point> _last_access = {};
    };
}
