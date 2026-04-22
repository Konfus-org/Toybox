#pragma once
#include "tbx/core/types/uuid.h"
#include "tbx/core/systems/graphics/color.h"
#include "tbx/core/systems/graphics/texture.h"
#include "tbx/core/systems/math/matrices.h"
#include "tbx/core/systems/math/quaternions.h"
#include "tbx/core/systems/math/vectors.h"
#include "tbx/core/tbx_api.h"
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace tbx
{
    class TBX_API Json
    {
      public:
        Json();
        Json(const std::string& data);
        Json(Json&& other) noexcept;
        Json& operator=(Json&& other);
        ~Json() noexcept;

        // Serializes the wrapped JSON value into a string.
        std::string to_string(int indent = 4) const;

        template <typename TValue>
        bool try_get(const std::string& key, TValue& out_value) const;

        template <typename TValue>
        bool try_get(const std::string& key, std::vector<TValue>& out_values) const;

        template <typename TValue>
        bool try_get(
            const std::string& key,
            std::size_t expected_size,
            std::vector<TValue>& out_values) const;

        // Attempts to retrieve a nested JSON object stored at the specified object key.
        bool try_get_child(const std::string& key, Json& out_value) const;

        // Attempts to retrieve an array of nested JSON objects stored at the specified object key.
        bool try_get_children(const std::string& key, std::vector<Json>& out_values) const;

      private:
        class Impl;
        std::unique_ptr<Impl> _data;
    };

}

#include "tbx/core/systems/files/json.inl"
