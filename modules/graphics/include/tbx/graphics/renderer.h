#pragma once
#include "tbx/common/handle.h"
#include "tbx/graphics/mesh.h"
#include "tbx/tbx_api.h"
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Provides a polymorphic base for renderer data payloads.
    /// </summary>
    /// <remarks>
    /// Ownership: Does not own external resources beyond derived payloads.
    /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
    /// </remarks>
    class TBX_API IRenderData
    {
      public:
        virtual ~IRenderData() = default;
    };

    /// <summary>
    /// Purpose: Stores asset-backed render data for static models and materials.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns asset handle copies for model and material references.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    class TBX_API StaticRenderData final : public IRenderData
    {
      public:
        StaticRenderData() = default;
        StaticRenderData(Handle model_handle, Handle material_handle)
            : model(std::move(model_handle))
            , material(std::move(material_handle))
        {
        }

        Handle model = {};
        Handle material = {};
    };

    /// <summary>
    /// Purpose: Stores procedural mesh data with optional material handles.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns mesh data and material handle copies.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    class TBX_API ProceduralData final : public IRenderData
    {
      public:
        ProceduralData() = default;
        ProceduralData(std::vector<Mesh> mesh_data, std::vector<Handle> material_handles)
            : meshes(std::move(mesh_data))
            , materials(std::move(material_handles))
        {
        }

        std::vector<Mesh> meshes = {};
        std::vector<Handle> materials = {};
    };

    /// <summary>
    /// Purpose: Associates render data with an entity for efficient rendering.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the render data payload via a unique pointer.
    /// Thread Safety: Safe to move between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API Renderer
    {
        Renderer() = default;
        Renderer(std::unique_ptr<IRenderData> render_data)
            : data(std::move(render_data))
        {
        }

        /// <summary>
        /// Purpose: Creates a renderer for a static model asset by name.
        /// </summary>
        /// <remarks>
        /// Ownership: Takes ownership of the created render data payload.
        /// Thread Safety: Safe to construct on any thread.
        /// </remarks>
        Renderer(std::string model_name, Handle material_handle = {})
            : data(
                  std::make_unique<StaticRenderData>(
                      std::move(model_name),
                      std::move(material_handle)))
        {
        }

        /// <summary>
        /// Purpose: Creates a renderer for a static model asset handle.
        /// </summary>
        /// <remarks>
        /// Ownership: Takes ownership of the created render data payload.
        /// Thread Safety: Safe to construct on any thread.
        /// </remarks>
        Renderer(Handle model_handle, Handle material_handle = {})
            : data(
                  std::make_unique<StaticRenderData>(
                      std::move(model_handle),
                      std::move(material_handle)))
        {
        }

        /// <summary>
        /// Purpose: Creates a renderer for static asset-backed render data.
        /// </summary>
        /// <remarks>
        /// Ownership: Takes ownership of the created render data payload.
        /// Thread Safety: Safe to construct on any thread.
        /// </remarks>
        Renderer(StaticRenderData render_data)
            : data(std::make_unique<StaticRenderData>(std::move(render_data)))
        {
        }

        /// <summary>
        /// Purpose: Creates a renderer for a single procedural mesh.
        /// </summary>
        /// <remarks>
        /// Ownership: Takes ownership of the created render data payload.
        /// Thread Safety: Safe to construct on any thread.
        /// </remarks>
        Renderer(const Mesh& mesh, Handle material_handle = {})
            : data(
                  std::make_unique<ProceduralData>(
                      std::vector<Mesh> {mesh},
                      std::vector<Handle> {std::move(material_handle)}))
        {
        }

        /// <summary>
        /// Purpose: Creates a renderer for procedural mesh data.
        /// </summary>
        /// <remarks>
        /// Ownership: Takes ownership of the created render data payload.
        /// Thread Safety: Safe to construct on any thread.
        /// </remarks>
        Renderer(ProceduralData render_data)
            : data(std::make_unique<ProceduralData>(std::move(render_data)))
        {
        }

        std::unique_ptr<IRenderData> data = {};
    };
}
