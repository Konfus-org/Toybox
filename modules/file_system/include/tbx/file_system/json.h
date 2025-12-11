#pragma once
#include "tbx/common/collections.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/common/string.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// <summary>
    /// Provides a lightweight wrapper around JSON parsing and serialization without exposing the
    /// underlying implementation.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the JSON value it wraps and transfers ownership when moved.
    /// Thread Safety: Not thread-safe; callers must synchronize external access.
    /// </remarks>
    class TBX_API Json
    {
      public:
        /// <summary>
        /// Constructs an empty JSON value.
        /// </summary>
        /// <remarks>
        /// Ownership: The Json instance owns the allocated JSON value.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        Json();

        /// <summary>
        /// Parses JSON content from text.
        /// </summary>
        /// <param name="data">UTF-8 JSON content to parse.</param>
        /// <remarks>
        /// Ownership: The Json instance owns the allocated JSON value.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        Json(const String& data);

        Json(Json&& other);
        Json& operator=(Json&& other);

        /// <summary>
        /// Releases the stored JSON data.
        /// </summary>
        /// <remarks>
        /// Ownership: Destroys the owned JSON value.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        ~Json();

        /// <summary>
        /// Serializes the wrapped JSON value into a string.
        /// </summary>
        /// <param name="indent">Indentation level for pretty-printing.</param>
        /// <returns>String representation of the JSON data.</returns>
        /// <remarks>
        /// Ownership: Returns a new String containing serialized JSON text.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        String to_string(int indent = 4) const;

        /// <summary>
        /// Attempts to retrieve an integer value stored at the specified object key.
        /// </summary>
        /// <param name="key">Name of the property to read.</param>
        /// <param name="out_value">Receives the parsed integer when the lookup succeeds.</param>
        /// <returns>True when the key exists and holds an integer value.</returns>
        /// <remarks>
        /// Ownership: The Json instance retains ownership of its internal value.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        bool try_get_int(const String& key, int& out_value) const;

        /// <summary>
        /// Attempts to retrieve a boolean value stored at the specified object key.
        /// </summary>
        /// <param name="key">Name of the property to read.</param>
        /// <param name="out_value">Receives the parsed boolean when the lookup succeeds.</param>
        /// <returns>True when the key exists and holds a boolean value.</returns>
        /// <remarks>
        /// Ownership: The Json instance retains ownership of its internal value.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        bool try_get_bool(const String& key, bool& out_value) const;

        /// <summary>
        /// Attempts to retrieve a floating-point value stored at the specified object key.
        /// </summary>
        /// <param name="key">Name of the property to read.</param>
        /// <param name="out_value">Receives the parsed floating-point value when the lookup succeeds.</param>
        /// <returns>True when the key exists and holds a floating-point value.</returns>
        /// <remarks>
        /// Ownership: The Json instance retains ownership of its internal value.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        bool try_get_float(const String& key, double& out_value) const;

        /// <summary>
        /// Attempts to retrieve a string value stored at the specified object key.
        /// </summary>
        /// <param name="key">Name of the property to read.</param>
        /// <param name="out_value">Receives the parsed string when the lookup succeeds.</param>
        /// <returns>True when the key exists and holds a string value.</returns>
        /// <remarks>
        /// Ownership: The Json instance retains ownership of its internal value.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        bool try_get_string(const String& key, String& out_value) const;

        /// <summary>
        /// Attempts to retrieve a list of string values stored at the specified object key.
        /// </summary>
        /// <param name="key">Name of the property to read.</param>
        /// <param name="out_values">Receives any parsed strings when the lookup succeeds.</param>
        /// <returns>True when the key exists and holds at least one string value.</returns>
        /// <remarks>
        /// Ownership: The Json instance retains ownership of its internal value.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        bool try_get_strings(const String& key, List<String>& out_values) const;

        /// <summary>
        /// Attempts to retrieve a list of integer values stored at the specified object key.
        /// </summary>
        /// <param name="key">Name of the property to read.</param>
        /// <param name="out_values">Receives any parsed integers when the lookup succeeds.</param>
        /// <returns>True when the key exists and holds at least one integer value.</returns>
        /// <remarks>
        /// Ownership: The Json instance retains ownership of its internal value.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        bool try_get_ints(const String& key, List<int>& out_values) const;

        /// <summary>
        /// Attempts to retrieve a list of boolean values stored at the specified object key.
        /// </summary>
        /// <param name="key">Name of the property to read.</param>
        /// <param name="out_values">Receives any parsed booleans when the lookup succeeds.</param>
        /// <returns>True when the key exists and holds at least one boolean value.</returns>
        /// <remarks>
        /// Ownership: The Json instance retains ownership of its internal value.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        bool try_get_bools(const String& key, List<bool>& out_values) const;

        /// <summary>
        /// Attempts to retrieve a list of floating-point values stored at the specified object key.
        /// </summary>
        /// <param name="key">Name of the property to read.</param>
        /// <param name="out_values">Receives any parsed floating-point values when the lookup succeeds.</param>
        /// <returns>True when the key exists and holds at least one floating-point value.</returns>
        /// <remarks>
        /// Ownership: The Json instance retains ownership of its internal value.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        bool try_get_floats(const String& key, List<double>& out_values) const;

        /// <summary>
        /// Attempts to retrieve a nested JSON object stored at the specified object key.
        /// </summary>
        /// <param name="key">Name of the property to read.</param>
        /// <param name="out_value">Receives the parsed Json when the lookup succeeds.</param>
        /// <returns>True when the key exists and holds an object value.</returns>
        /// <remarks>
        /// Ownership: The Json instance retains ownership of its internal value.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        bool try_get_child(const String& key, Json& out_value) const;

        /// <summary>
        /// Attempts to retrieve an array of nested JSON objects stored at the specified object key.
        /// </summary>
        /// <param name="key">Name of the property to read.</param>
        /// <param name="out_values">Receives any parsed Json objects when the lookup succeeds.</param>
        /// <returns>True when the key exists and holds at least one object entry.</returns>
        /// <remarks>
        /// Ownership: The Json instance retains ownership of its internal value.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        bool try_get_children(const String& key, List<Json>& out_values) const;

      private:
        class Impl;
        Scope<Impl> _data;
    };
}
