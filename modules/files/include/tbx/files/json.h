#pragma once
#include "tbx/common/uuid.h"
#include "tbx/graphics/color.h"
#include "tbx/graphics/texture.h"
#include "tbx/math/matrices.h"
#include "tbx/math/quaternions.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"
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

#include "tbx/files/json.inl"
