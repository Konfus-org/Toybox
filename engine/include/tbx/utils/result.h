#pragma once
#include "tbx/tbx_api.h"
#include <memory>
#include <string>

namespace tbx
{
    // TODO: Make all constructors use explicit except for Result, Handle, and Uuid. They should be
    // explicit(false) to allow them to be implicitly created from bool, string or uint, and uint
    // respectively
    class TBX_API Result
    {
      public:
        explicit Result();
        explicit(false) Result(bool success, std::string report = "");

        // Returns true if the result indicates success.
        bool succeeded() const;

        // Marks the result as a success. Report is optional.
        void ok(std::string report = "") const;

        // Marks the result as a failure. A report is required on failure.
        void failure(std::string report) const;

        // Returns the report associated with the result.
        const std::string& get_report() const;

      public:
        operator bool() const
        {
            return succeeded();
        }

      public:
        static const Result OK;
        static const Result FAILURE;

      private:
        std::shared_ptr<bool> _success;
        std::shared_ptr<std::string> _report;
    };
}
