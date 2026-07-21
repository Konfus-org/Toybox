#pragma once
#include "tbx/core/typedefs.h"

namespace tbx::gpu
{
    /// @brief
    /// Purpose: GPU mesh — RAII: the destructor (defined by the selected gfx backend) releases
    /// the buffers. Obtain via upload_mesh().
    class Mesh final
    {
      public:
        Mesh(uint32 vertex_array, uint32 vertex_buffer, int vertex_count)
            : _vertex_array(vertex_array)
            , _vertex_buffer(vertex_buffer)
            , _vertex_count(vertex_count)
        {
        }
        ~Mesh();

      public:
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

      public:
        /// @brief
        /// Purpose: Backend-native vertex array id.
        uint32 get_vertex_array() const
        {
            return _vertex_array;
        }

        /// @brief
        /// Purpose: Backend-native vertex buffer id.
        uint32 get_vertex_buffer() const
        {
            return _vertex_buffer;
        }

        /// @brief
        /// Purpose: Number of vertices to draw.
        int get_vertex_count() const
        {
            return _vertex_count;
        }

      private:
        uint32 _vertex_array = 0;
        uint32 _vertex_buffer = 0;
        int _vertex_count = 0;
    };
}
