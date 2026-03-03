#pragma once
#include "opengl_resource.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include <chrono>
#include <memory>
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
        tbx::Uuid add_texture(
            const tbx::Texture& texture,
            const tbx::Uuid& resource_uuid,
            bool pin = false);
        tbx::Uuid add_texture(const tbx::Handle& texture_handle, bool pin = false);
        bool try_get(const tbx::Uuid& resource_uuid, std::shared_ptr<IOpenGlResource>& out_resource)
            const;

        void pin(const tbx::Uuid& resource_uuid);
        void unpin(const tbx::Uuid& resource_uuid);

        void clear();
        void clear_unused();

      private:
        tbx::AssetManager& _asset_manager;
        std::unordered_map<tbx::Uuid, std::shared_ptr<IOpenGlResource>> _resources = {};
        std::unordered_map<tbx::Uuid, std::shared_ptr<IOpenGlResource>> _pinned_resources = {};
        std::unordered_map<tbx::Uuid, std::chrono::steady_clock::time_point> _last_access = {};
    };
}
