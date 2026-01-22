#pragma once
#include "opengl_buffers.h"
#include "opengl_resource.h"
#include "tbx/common/int.h"
#include "tbx/graphics/mesh.h"
#include "tbx/tbx_api.h"

namespace tbx::plugins
{
    /// <summary>OpenGL implementation of a mesh resource.</summary>
    /// <remarks>Purpose: Manages a VAO with vertex and index buffers for drawing.
    /// Ownership: Owns the VAO and associated buffers.
    /// Thread Safety: Not thread-safe; use on the render thread.</remarks>
    class OpenGlMesh final : public IGlResource
    {
      public:
        /// <summary>Creates an OpenGL mesh resource from CPU mesh data.</summary>
        /// <remarks>Purpose: Initializes GPU buffers for the mesh.
        /// Ownership: Owns the created VAO and buffer resources.
        /// Thread Safety: Construct on the render thread.</remarks>
        explicit OpenGlMesh(const Mesh& mesh);

        /// <summary>Destroys the OpenGL mesh resource.</summary>
        /// <remarks>Purpose: Releases the VAO and buffer resources.
        /// Ownership: Owns the GPU handles being destroyed.
        /// Thread Safety: Destroy on the render thread.</remarks>
        ~OpenGlMesh() noexcept override;

        /// <summary>Uploads a vertex buffer to the mesh.</summary>
        /// <remarks>Purpose: Updates vertex data for the mesh.
        /// Ownership: Copies data to the GPU; caller retains CPU ownership.
        /// Thread Safety: Call only on the render thread.</remarks>
        void set_vertex_buffer(const VertexBuffer& buffer);

        /// <summary>Uploads an index buffer to the mesh.</summary>
        /// <remarks>Purpose: Updates index data for the mesh.
        /// Ownership: Copies data to the GPU; caller retains CPU ownership.
        /// Thread Safety: Call only on the render thread.</remarks>
        void set_index_buffer(const IndexBuffer& buffer);

        /// <summary>Issues a draw call for the mesh.</summary>
        /// <remarks>Purpose: Draws indexed triangles for the mesh.
        /// Ownership: Does not transfer ownership of any resources.
        /// Thread Safety: Call only on the render thread.</remarks>
        void draw();

        /// <summary>Binds the mesh's VAO and buffers.</summary>
        /// <remarks>Purpose: Binds the VAO and buffers for rendering.
        /// Ownership: The mesh retains ownership of its GPU handles.
        /// Thread Safety: Call only on the render thread.</remarks>
        void bind() override;

        /// <summary>Unbinds the mesh's VAO and buffers.</summary>
        /// <remarks>Purpose: Unbinds the VAO and buffers.
        /// Ownership: The mesh retains ownership of its GPU handles.
        /// Thread Safety: Call only on the render thread.</remarks>
        void unbind() override;

      private:
        uint32 _vertex_array_id = 0;
        OpenGlVertexBuffer _vertex_buffer;
        OpenGlIndexBuffer _index_buffer;
    };
}
