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
        tbx::Uuid add_material(const tbx::MaterialInstance& material, bool pin = false);
        tbx::Uuid add_material(
            const std::shared_ptr<OpenGlShaderProgram>& shader_program,
            const tbx::Uuid& resource_uuid,
            bool pin = false);
        tbx::Uuid add_static_mesh(const tbx::StaticMesh& static_mesh, bool pin = false);
        tbx::Uuid add_texture(const tbx::Handle& texture_handle, bool pin = false);
        tbx::Uuid add_texture(
            const tbx::Texture& texture,
            const tbx::Uuid& resource_uuid,
            bool pin = false);
        void clear();
        void clear_unused();
        std::shared_ptr<tbx::Material> get_material_asset(const tbx::Handle& material_handle);
        void on_asset_reloaded(const tbx::Handle& asset_handle);
        void pin(const tbx::Uuid& resource_uuid);

        template <typename TResource>
        bool try_get(const tbx::Uuid& resource_uuid, std::shared_ptr<TResource>& out_resource) const;

        void unpin(const tbx::Uuid& resource_uuid);

      private:
        void clear_shader_dependencies(const tbx::Uuid& program_key);
        void invalidate_material_program(const tbx::Uuid& program_key);
        void invalidate_resource(const tbx::Uuid& resource_uuid);
        void store_shader_dependencies(
            const tbx::Uuid& program_key,
            const std::vector<tbx::Handle>& shader_handles);
        bool try_get_raw(
            const tbx::Uuid& resource_uuid,
            std::shared_ptr<IOpenGlResource>& out_resource) const;

      private:
        tbx::AssetManager& _asset_manager;
        std::unordered_map<tbx::Uuid, std::shared_ptr<IOpenGlResource>> _resources = {};
        std::unordered_map<tbx::Uuid, std::weak_ptr<tbx::Mesh>> _dynamic_mesh_sources = {};
        std::unordered_map<tbx::Uuid, std::shared_ptr<tbx::Material>> _material_assets = {};
        std::unordered_map<tbx::Uuid, std::shared_ptr<IOpenGlResource>> _pinned_resources = {};
        std::unordered_map<tbx::Uuid, std::chrono::steady_clock::time_point> _last_access = {};
        std::unordered_map<tbx::Uuid, std::vector<tbx::Uuid>> _programs_by_shader = {};
        std::unordered_map<tbx::Uuid, std::vector<tbx::Uuid>> _shaders_by_program = {};
    };
}

#include "opengl_resource_manager.inl"
