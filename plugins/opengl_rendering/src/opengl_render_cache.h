#pragma once
#include "gl_id_provider.h"
#include "opengl_material.h"
#include "opengl_mesh.h"
#include "opengl_model.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/common/handle.h"
#include "tbx/common/uuid.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/model.h"
#include "tbx/graphics/shader.h"
#include "tbx/graphics/texture.h"
#include <memory>
#include <unordered_map>

namespace tbx::plugins
{
    /// <summary>OpenGL resource cache for rendering.</summary>
    /// <remarks>
    /// Purpose: Owns cached OpenGL resources derived from Toybox assets.
    /// Ownership: Owns cached OpenGL resource instances and fallback materials.
    /// Thread Safety: Not thread-safe; use on the render thread.
    /// </remarks>
    class OpenGlRenderCache final
    {
      public:
        /// <summary>Clears all cached resources.</summary>
        /// <remarks>
        /// Purpose: Releases cached shared resources and resets fallback state.
        /// Ownership: Releases owned cache entries.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        void clear();

        /// <summary>Returns the cached model for a handle, creating it if needed.</summary>
        /// <remarks>
        /// Purpose: Converts model assets to OpenGL-ready model data.
        /// Ownership: Returns a pointer to cached data owned by the cache.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        OpenGlModel* get_cached_model(
            AssetManager& asset_manager,
            const Handle& handle,
            const GlIdProvider& id_provider);

        /// <summary>Returns the cached material for a handle, creating it if needed.</summary>
        /// <remarks>
        /// Purpose: Builds or retrieves OpenGL material data.
        /// Ownership: Returns a pointer to cached data owned by the cache.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        OpenGlMaterial* get_cached_material(
            AssetManager& asset_manager,
            const Handle& handle,
            const std::shared_ptr<Material>& fallback_material,
            const GlIdProvider& id_provider);

        /// <summary>Returns the fallback material used when assets are unavailable.</summary>
        /// <remarks>
        /// Purpose: Provides a stable fallback material for rendering.
        /// Ownership: Returns a pointer to cached data owned by the cache.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        OpenGlMaterial* get_cached_fallback_material(
            AssetManager& asset_manager,
            const std::shared_ptr<Material>& fallback_material,
            const GlIdProvider& id_provider);

        /// <summary>Ensures a shader program is cached for the given shader handle.</summary>
        /// <remarks>
        /// Purpose: Loads and links shader programs into the OpenGL cache.
        /// Ownership: Returns a shared pointer owned by the cache.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        std::shared_ptr<OpenGlShaderProgram> ensure_shader_program(
            AssetManager& asset_manager,
            Handle& shader_handle,
            const GlIdProvider& id_provider);

        /// <summary>Returns the cached mesh for the provided cache key.</summary>
        /// <remarks>
        /// Purpose: Retrieves cached OpenGL mesh resources.
        /// Ownership: Returns a shared pointer owned by the cache.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        std::shared_ptr<OpenGlMesh> get_cached_mesh(const Uuid& mesh_key) const;

        /// <summary>Returns the cached shader program for an id, if present.</summary>
        /// <remarks>
        /// Purpose: Retrieves shader program resources for rendering.
        /// Ownership: Returns a shared pointer owned by the cache.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        std::shared_ptr<OpenGlShaderProgram> get_cached_shader_program(
            const Uuid& shader_id) const;

        /// <summary>Returns the cached material for a material id, if present.</summary>
        /// <remarks>
        /// Purpose: Looks up cached material overrides by id.
        /// Ownership: Returns a pointer to cached data owned by the cache.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        const OpenGlMaterial* get_cached_material(const Uuid& material_id) const;

        /// <summary>Returns the cached texture for a texture id, if present.</summary>
        /// <remarks>
        /// Purpose: Provides texture resources for material bindings.
        /// Ownership: Returns a shared pointer owned by the cache.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        std::shared_ptr<OpenGlTexture> get_cached_texture(const Uuid& texture_id) const;

        /// <summary>Returns the cached mesh, creating one if needed.</summary>
        /// <remarks>
        /// Purpose: Converts mesh data into OpenGL mesh resources.
        /// Ownership: Returns a shared pointer owned by the cache.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        std::shared_ptr<OpenGlMesh> get_mesh(const Mesh& mesh, const Uuid& mesh_key);

        /// <summary>Returns the cached shader stage for a key, creating one if needed.</summary>
        /// <remarks>
        /// Purpose: Converts shader sources into compiled OpenGL shaders.
        /// Ownership: Returns a shared pointer owned by the cache.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        std::shared_ptr<OpenGlShader> get_shader(
            const ShaderSource& shader,
            const Uuid& shader_key);

        /// <summary>Returns the cached shader program for a shader asset.</summary>
        /// <remarks>
        /// Purpose: Links shader stages into a program resource.
        /// Ownership: Returns a shared pointer owned by the cache.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        std::shared_ptr<OpenGlShaderProgram> get_shader_program(
            const Uuid& shader_id,
            const Shader& shader,
            const GlIdProvider& id_provider);

        /// <summary>Returns the default white texture resource.</summary>
        /// <remarks>
        /// Purpose: Supplies a fallback texture for missing bindings.
        /// Ownership: Returns a shared pointer owned by the cache.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        std::shared_ptr<OpenGlTexture> get_default_texture();

        /// <summary>Returns the cached texture for an asset.</summary>
        /// <remarks>
        /// Purpose: Converts texture assets into OpenGL textures.
        /// Ownership: Returns a shared pointer owned by the cache.
        /// Thread Safety: Call only on the render thread.
        /// </remarks>
        std::shared_ptr<OpenGlTexture> get_texture(const Uuid& texture_id, const Texture& texture);

      private:
        bool try_build_gl_material(
            AssetManager& asset_manager,
            const Material& source_material,
            OpenGlMaterial& out_material,
            const GlIdProvider& id_provider);

        std::unordered_map<Uuid, std::shared_ptr<OpenGlMesh>> _meshes = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlShader>> _shaders = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlShaderProgram>> _shader_programs = {};
        std::unordered_map<Uuid, std::shared_ptr<OpenGlTexture>> _textures = {};
        std::unordered_map<Uuid, OpenGlModel> _models = {};
        std::unordered_map<Uuid, OpenGlMaterial> _materials = {};
        OpenGlMaterial _fallback_material = {};
        bool _has_fallback_material = false;
        std::shared_ptr<OpenGlTexture> _default_texture = {};
    };
}
