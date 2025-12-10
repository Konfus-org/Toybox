#pragma once
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
    /// Ownership: Owns the JSON value it wraps.
    /// Thread Safety: Not thread-safe; callers must synchronize external access.
    /// </remarks>
    class TBX_API Json
    {
      public:
        /// <summary>
        /// Parses JSON content from text.
        /// </summary>
        /// <param name="data">UTF-8 JSON content to parse.</param>
        /// <remarks>
        /// Ownership: The Json instance owns the allocated JSON value.
        /// Thread Safety: Not thread-safe.
        /// </remarks>
        Json(const String& data);

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

      private:
        class Impl;
        Scope<Impl> _data;
    };
}
