#pragma once
#include "tbx/tbx_api.h"
#include <string>

namespace tbx
{
    // TODO: Make all constructors use explicit except for Result, Handle, and Uuid. They should be
    // explicit(false) to allow them to be implicitly created from bool, string or uint, and uint
    // respectively
    /// @brief
    /// Purpose: Reports the success/failure of an operation along with an optional human-readable
    /// report.
    /// @details
    /// Ownership: Value type holding its own success flag and report string inline (no heap
    /// indirection). Copies are fully independent.
    /// Thread Safety: A single instance is not thread-safe; copy before sharing across threads. The
    /// success/report are mutable so the result of an operation can be reported through a const
    /// reference (e.g. a message's `result` member).
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
        // Mutable so an operation's outcome can be reported through a const reference. Stored inline
        // (no shared_ptr) so copies are independent — copying Result::OK and flagging it failed no
        // longer corrupts the shared instance.
        mutable bool _success = true;
        mutable std::string _report = {};
    };
}
