#pragma once
#include "opengl_mesh.h"
#include "opengl_resource.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/uuid.h"
#include "tbx/ecs/entity.h"
#include "tbx/graphics/renderer.h"
#include "tbx/graphics/shader.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace opengl_rendering
{
    using namespace tbx;
    /// <summary>
    /// Purpose: Owns OpenGL resource caches and unloads entries that have not been referenced
    /// recently.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores shared ownership of cached resources keyed by entity UUID and stores a
    /// non-owning reference to AssetManager for source asset loading.
    /// Thread Safety: Not thread-safe; call only from the render thread.
    /// </remarks>
    /*class OpenGlResourceManager final
    {
      public:

     * OpenGlResourceManager(AssetManager&




     * * * * * asset_manager);
        bool try_get(

     * const Entity& entity,

     *
 *


     * * * std::vector<IOpenGlResource> out_resources,

     * bool pin = false);

 template


     * *

     * * * <typename ResourceType, typename...
     * TArgs>
        Uuid add(TArgs&&... args);


     * * template


     * * * <typename
     * ResourceType, typename... TArgs>
        bool add(const
     * Uuid&
     * resource_uuid,



     * * * * TArgs&&... args);
        bool add(const Entity&
     * entity, bool pin
     * =
     * false);

 bool

 * *
     * try_get(
            const Entity&
     * entity,

     *
     * std::shared_ptr<IOpenGlResource>&


     * * * out_resource,
            bool
     * pin =
     * false);

     * bool try_get(const Uuid&
     *
     * resource_uuid,
     *
     *
     *
     * std::shared_ptr<IOpenGlResource>& out_resource)
            const;



     * * void
     *

     * * pin(const
     * Uuid& resource_uuid);
        void unpin(const Uuid&
     *
     *
     *
     * resource_uuid);


     * void clear();

     * void clear_unused();
    };*/
}
