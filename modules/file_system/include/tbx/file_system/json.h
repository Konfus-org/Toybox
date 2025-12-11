#pragma once
#include "tbx/common/collections.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/common/string.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // Provides a lightweight wrapper around JSON parsing and serialization without exposing the
    // underlying implementation.
    // Ownership: Owns the JSON value it wraps and transfers ownership when moved.
    // Thread Safety: Not thread-safe; callers must synchronize external access.
    class TBX_API Json
    {
      public:
        Json();
        Json(const String& data);
        ~Json();

        // Serializes the wrapped JSON value into a string.
        // Ownership: Returns a new String containing serialized JSON text.
        // Thread Safety: Not thread-safe.
        String to_string(int indent = 4) const;

        // Attempts to retrieve an integer value stored at the specified object key.
        // Ownership: The Json instance retains ownership of its internal value.
        // Thread Safety: Not thread-safe.
        bool try_get_int(const String& key, int& out_value) const;

        // Attempts to retrieve a boolean value stored at the specified object key.
        // Ownership: The Json instance retains ownership of its internal value.
        // Thread Safety: Not thread-safe.
        bool try_get_bool(const String& key, bool& out_value) const;

        // Attempts to retrieve a floating-point value stored at the specified object key.
        // Ownership: The Json instance retains ownership of its internal value.
        // Thread Safety: Not thread-safe.
        bool try_get_float(const String& key, double& out_value) const;

        // Attempts to retrieve a string value stored at the specified object key.
        // Ownership: The Json instance retains ownership of its internal value.
        // Thread Safety: Not thread-safe.
        bool try_get_string(const String& key, String& out_value) const;

        // Attempts to retrieve a list of string values stored at the specified object key.
        // Ownership: The Json instance retains ownership of its internal value.
        // Thread Safety: Not thread-safe.
        bool try_get_strings(const String& key, List<String>& out_values) const;

        // Attempts to retrieve a list of integer values stored at the specified object key.
        // Ownership: The Json instance retains ownership of its internal value.
        // Thread Safety: Not thread-safe.
        bool try_get_ints(const String& key, List<int>& out_values) const;

        // Attempts to retrieve a list of boolean values stored at the specified object key.
        // Ownership: The Json instance retains ownership of its internal value.
        // Thread Safety: Not thread-safe.
        bool try_get_bools(const String& key, List<bool>& out_values) const;

        // Attempts to retrieve a list of floating-point values stored at the specified object key.
        // Ownership: The Json instance retains ownership of its internal value.
        // Thread Safety: Not thread-safe.
        bool try_get_floats(const String& key, List<double>& out_values) const;

        // Attempts to retrieve a nested JSON object stored at the specified object key.
        // Ownership: The Json instance retains ownership of its internal value.
        // Thread Safety: Not thread-safe.
        bool try_get_child(const String& key, Json& out_value) const;

        // Attempts to retrieve an array of nested JSON objects stored at the specified object key.
        // Ownership: The Json instance retains ownership of its internal value.
        // Thread Safety: Not thread-safe.
        bool try_get_children(const String& key, List<Json>& out_values) const;

      private:
        class Impl;
        Scope<Impl> _data;
    };
}
