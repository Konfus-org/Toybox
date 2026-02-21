#pragma once
#include "tbx/tbx_api.h"
#include <memory>
#include <string>

namespace tbx
{
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
        std::shared_ptr<bool> _success;
        std::shared_ptr<std::string> _report;
    };
}
