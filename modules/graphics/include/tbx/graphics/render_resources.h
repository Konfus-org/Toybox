#pragma once
#include "tbx/assets/manager.h"
#include "tbx/common/handle.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/texture.h"
#include "tbx/tbx_api.h"
#include <chrono>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx
{
    class IGraphicsBackend;

    /// @brief
    /// Purpose: Tracks backend resource lifetimes for meshes, materials, and textures.
    /// @details
    /// Ownership: Borrows the asset manager and backend while owning cached resource metadata.
    /// Thread Safety: Not inherently thread-safe; callers must synchronize external access.
    class TBX_API RenderResourceManager
    {
      public:
        RenderResourceManager(AssetManager& asset_manager, IGraphicsBackend& backend);

      public:
        Uuid upload_dynamic_mesh(const DynamicMesh& dynamic_mesh, bool pin = false);
        Uuid upload_static_mesh(const StaticMesh& static_mesh, bool pin = false);
        Uuid upload_material(const MaterialInstance& material_instance, bool pin = false);
        Uuid upload_texture(const Texture& texture, const Uuid& resource_uuid, bool pin = false);
        Uuid upload_texture(const Handle& texture_handle, bool pin = false);
        Uuid upload_render_texture(const TextureSettings& texture_settings, bool pin = false);
        void unload(const Uuid& resource_uuid);
        void on_asset_reloaded(const Handle& asset_handle);
        void pin(const Uuid& resource_uuid);
        void unpin(const Uuid& resource_uuid);
        void clear();
        void clear_unused();
    };
}
