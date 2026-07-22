#pragma once
#include "tbx/api.h"
#include "tbx/utils/typedefs.h"

namespace tbx::gpu
{
    /// @brief
    /// Purpose: GPU 2D texture (RGBA8) — RAII: the backend-defined destructor releases it.
    /// Obtain via upload_texture().
    class TBX_API Texture2d final
    {
      public:
        explicit Texture2d(uint32 id)
            : _id(id)
        {
        }
        ~Texture2d();

      public:
        Texture2d(const Texture2d&) = delete;
        Texture2d& operator=(const Texture2d&) = delete;

      public:
        /// @brief
        /// Purpose: Backend-native texture id.
        uint32 get_id() const
        {
            return _id;
        }

      private:
        uint32 _id = 0;
    };
}
