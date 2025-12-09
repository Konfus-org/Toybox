#pragma once
#include "tbx/common/smart_pointers.h"
#include "tbx/tbx_api.h"
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
        const std::string& get_report() const;

        operator bool() const
        {
            return succeeded();
        }

      private:
        Ref<bool> _success;
        Ref<std::string> _report;
    };
}
