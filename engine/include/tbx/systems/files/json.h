#pragma once
#include "tbx/systems/files/serialization.h"
#include "tbx/tbx_api.h"
#include "tbx/types/color.h"
#include "tbx/types/handle.h"
#include "tbx/types/matrices.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/shader.h"
#include "tbx/types/texture.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/types/vectors.h"
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
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

        static Json parse(const std::string& data);

        template <typename TValue>
        bool try_get(TValue& out_value) const;

        template <typename TValue>
        bool try_update(TValue& in_out_value) const;

        template <typename TValue>
        bool try_get(const std::string& key, TValue& out_value) const;

        template <typename TValue>
        bool try_get(const std::string& key, std::vector<TValue>& out_values) const;

        template <typename TValue>
        bool try_get(const std::string& key, size expected_size, std::vector<TValue>& out_values)
            const;

        // Attempts to retrieve a nested JSON object stored at the specified object key.
        bool try_get_child(const std::string& key, Json& out_value) const;

        // Attempts to retrieve an array of nested JSON objects stored at the specified object key.
        bool try_get_children(const std::string& key, std::vector<Json>& out_values) const;

      private:
        bool try_get_raw(std::string& out_value) const;
        bool try_get_raw(const std::string& key, std::string& out_value) const;

        template <typename TValue>
        bool try_get_nlohmann(TValue& out_value) const;

        template <typename TValue>
        bool try_update_nlohmann(TValue& in_out_value) const;

        template <typename TValue>
        bool try_get_nlohmann(const std::string& key, TValue& out_value) const;

        class Impl;
        std::unique_ptr<Impl> _data;
    };

}

#include "tbx/systems/files/json.inl"
