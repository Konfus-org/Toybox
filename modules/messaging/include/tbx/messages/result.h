#pragma once
#include "tbx/tbx_api.h"
#include "tbx/tsl/smart_pointers.h"
#include "tbx/tsl/string.h"
#include <string>

namespace tbx
{
    // Lightweight shared result that captures success/failure and a human-readable message.
    // Ownership: Result instances share their underlying state via shared_ptr so copies remain
    // synchronized. Thread-safety: safe for concurrent reads; callers must synchronize writes.
    class TBX_API Result
    {
      public:
        Result();

        // Returns true if the result indicates success.
        bool succeeded() const;

        // Marks the result as a success. Report is optional.
        void flag_success(std::string report = "") const;
        // Marks the result as a failure. A report is required on failure.
        void flag_failure(std::string report) const;

        // Returns the report associated with the result.
        const String& get_report() const;

        operator bool() const
        {
            return succeeded();
        }

      private:
        void ensure_success();
        void ensure_message() const;

        Ref<bool> _success;
        Ref<String> _report;
    };
}
