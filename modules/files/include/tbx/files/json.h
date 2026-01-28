#pragma once
#include "tbx/common/uuid.h"
#include "tbx/tbx_api.h"
#include <memory>
#include <string>
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

        // Attempts to retrieve an integer value stored at the specified object key.
        bool try_get_int(const std::string& key, int& out_value) const;

        // Attempts to retrieve a boolean value stored at the specified object key.
        bool try_get_bool(const std::string& key, bool& out_value) const;

        // Attempts to retrieve a floating-point value stored at the specified object key.
        bool try_get_float(const std::string& key, double& out_value) const;

        // Attempts to retrieve a string value stored at the specified object key.
        bool try_get_string(const std::string& key, std::string& out_value) const;

        // Attempts to retrieve a UUID value stored at the specified object key.
        bool try_get_uuid(const std::string& key, Uuid& out_value) const;

        // Attempts to retrieve a list of string values stored at the specified object key.
        bool try_get_strings(const std::string& key, std::vector<std::string>& out_values) const;

        // Attempts to retrieve a list of integer values stored at the specified object key.
        bool try_get_ints(const std::string& key, std::vector<int>& out_values) const;

        // Attempts to retrieve a list of boolean values stored at the specified object key.
        bool try_get_bools(const std::string& key, std::vector<bool>& out_values) const;

        // Attempts to retrieve a list of floating-point values stored at the specified object key.
        bool try_get_floats(const std::string& key, std::vector<double>& out_values) const;

        // Attempts to retrieve a nested JSON object stored at the specified object key.
        bool try_get_child(const std::string& key, Json& out_value) const;

        // Attempts to retrieve an array of nested JSON objects stored at the specified object key.
        bool try_get_children(const std::string& key, std::vector<Json>& out_values) const;

      private:
        class Impl;
        std::unique_ptr<Impl> _data;
    };
}
