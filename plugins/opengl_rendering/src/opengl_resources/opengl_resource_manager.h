#pragma once
#include "opengl_resource.h"
#include "opengl_shader.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/uuid.h"
#include "tbx/ecs/entity.h"
#include <chrono>
#include <memory>

namespace opengl_rendering
{
    /// <summary>
    /// Purpose: Stores texture bindings for OpenGL material rendering.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the binding name; references GPU textures by UUID.
    /// Thread Safety: Not thread-safe; use on the render thread.
    /// </remarks>
    struct OpenGlMaterialTexture
    {
        std::string name = "";
        tbx::Uuid texture_id = {};
    };

    /// <summary>
    /// Purpose: Represents a GPU-ready material cache for OpenGL rendering.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns parameter and binding data; references GPU resources by UUID.
    /// Thread Safety: Not thread-safe; use on the render thread.
    /// </remarks>
    struct OpenGlMaterial
    {
        tbx::Uuid shader_program = {};
        std::vector<tbx::MaterialParameter> parameters = {};
        std::vector<OpenGlMaterialTexture> textures = {};
    };

    struct OpenGlDrawResources final
    {
        tbx::Uuid mesh = tbx::Uuid::NONE;
        OpenGlMaterial material = {};

        void draw() const;
    };

    class OpenGlResourceManager final
    {
      public:
        OpenGlResourceManager(tbx::AssetManager& asset_manager);

        bool add(const tbx::Entity& entity, bool pin = false);
        bool try_get(
            const tbx::Entity& entity,
            OpenGlDrawResources& out_resources,
            bool pin = false);
        bool try_get(const tbx::Uuid& resource_uuid, std::shared_ptr<IOpenGlResource>& out_resource)
            const;
        bool try_get(
            const tbx::Entity& entity,
            std::shared_ptr<IOpenGlResource>& out_resource,
            bool pin = false);

        void pin(const tbx::Uuid& resource_uuid);
        void unpin(const tbx::Uuid& resource_uuid);

        void clear();
        void clear_unused();

      private:
        tbx::AssetManager& _asset_manager;
        std::unordered_map<tbx::Uuid, std::shared_ptr<IOpenGlResource>> _resources = {};
        std::unordered_map<tbx::Uuid, std::shared_ptr<IOpenGlResource>> _pinned_resources = {};
        std::unordered_map<tbx::Uuid, OpenGlDrawResources> _resources_by_entity = {};
        std::unordered_map<tbx::Uuid, std::chrono::steady_clock::time_point> _last_access = {};
    };
}
