#pragma once
#include "opengl_buffers.h"
#include "opengl_resource.h"
#include "tbx/common/int.h"
#include "tbx/graphics/mesh.h"
#include "tbx/math/matrices.h"
#include "tbx/tbx_api.h"
#include <vector>

namespace opengl_rendering
{
    struct OpenGlMeshInstanceData
    {
        tbx::Mat4 model = tbx::Mat4(1.0F);
        tbx::uint32 instance_id = 0;
    };

    /// @brief
    /// Purpose: Manages a VAO with vertex and index buffers for drawing.
    /// @details
    /// Ownership: Owns the VAO and associated buffers.
    /// Thread Safety: Not thread-safe; use on the render thread.
    class OpenGlMesh final : public IOpenGlResource
    {
      public:
        explicit OpenGlMesh(const tbx::Mesh& mesh);
        OpenGlMesh(const OpenGlMesh&) = delete;
        OpenGlMesh& operator=(const OpenGlMesh&) = delete;
        OpenGlMesh(OpenGlMesh&& other) noexcept;
        OpenGlMesh& operator=(OpenGlMesh&& other) noexcept;
        ~OpenGlMesh() noexcept override;

        /// @brief
        /// Purpose: Updates vertex data for the mesh.
        /// @details
        /// Ownership: Copies data to the GPU; caller retains CPU ownership.
        /// Thread Safety: Call only on the render thread.
        void set_vertex_buffer(const tbx::VertexBuffer& buffer);

        /// @brief
        /// Purpose: Updates index data for the mesh.
        /// @details
        /// Ownership: Copies data to the GPU; caller retains CPU ownership.
        /// Thread Safety: Call only on the render thread.
        void set_index_buffer(const tbx::IndexBuffer& buffer);

        /// @brief
        /// Purpose: Draws indexed triangles or patches for the mesh.
        /// @details
        /// Ownership: Does not transfer ownership of any resources.
        /// Thread Safety: Call only on the render thread.
        void draw() const;

        /// @brief
        /// Purpose: Draws indexed triangles without rebinding the VAO, enabling pass-level binding
        /// caches to reduce driver overhead.
        /// @details
        /// Ownership: Does not transfer ownership of any resources.
        /// Thread Safety: Call only on the render thread while this mesh remains bound.
        void draw_bound() const;

        /// @brief
        /// Purpose: Draws indexed triangles or patches across multiple instances.
        /// @details
        /// Ownership: Does not transfer ownership of any resources.
        /// Thread Safety: Call only on the render thread.
        void upload_instance_data(
            const std::vector<OpenGlMeshInstanceData>& instances,
            int instance_model_attribute_location,
            int instance_id_attribute_location);
        void draw_instanced(tbx::uint32 instance_count) const;

        /// @brief
        /// Purpose: Draws indexed triangles across multiple instances without rebinding the VAO,
        /// enabling pass-level binding caches to reduce driver overhead.
        /// @details
        /// Ownership: Does not transfer ownership of any resources.
        /// Thread Safety: Call only on the render thread while this mesh remains bound.
        void draw_instanced_bound(tbx::uint32 instance_count) const;

        /// @brief
        /// Purpose: Binds the VAO and buffers for rendering.
        /// @details
        /// Ownership: The mesh retains ownership of its GPU handles.
        /// Thread Safety: Call only on the render thread.
        void bind() override;

        /// @brief
        /// Purpose: Unbinds the VAO and buffers.
        /// @details
        /// Ownership: The mesh retains ownership of its GPU handles.
        /// Thread Safety: Call only on the render thread.
        void unbind() override;

      private:
        tbx::uint32 _vertex_array_id = 0;
        tbx::uint32 _instance_buffer_id = 0;
        int _instance_model_attribute_location = -1;
        int _instance_id_attribute_location = -1;
        OpenGlVertexBuffer _vertex_buffer;
        OpenGlIndexBuffer _index_buffer;
    };
}
