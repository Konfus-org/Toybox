#pragma once
#include "tbx/common/smart_pointers.h"
#include "tbx/common/string.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    class TBX_API Result
    {
      public:
        Result();

        // Returns true if the result indicates success.
        bool succeeded() const;

        // Marks the result as a success. Report is optional.
        void flag_success(String report = "") const;

        // Marks the result as a failure. A report is required on failure.
        void flag_failure(String report) const;

        // Returns the report associated with the result.
        const String& get_report() const;

        operator bool() const
        {
            return succeeded();
        }

      private:
        Ref<bool> _success;
        Ref<String> _report;
    };
}
