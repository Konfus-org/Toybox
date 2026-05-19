#pragma once
#include "tbx/systems/async/cancellation_token.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <any>
#include <memory>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Represents one payload-driven CPU pipeline operation.
    /// @details
    /// Ownership: Implementations own their operation state. Payload ownership stays with caller.
    /// Thread Safety: Depends on implementation.
    class TBX_API PipelineOperation
    {
      public:
        PipelineOperation() = default;
        virtual ~PipelineOperation() noexcept = default;

      public:
        PipelineOperation(const PipelineOperation&) = delete;
        PipelineOperation& operator=(const PipelineOperation&) = delete;
        PipelineOperation(PipelineOperation&&) noexcept = delete;
        PipelineOperation& operator=(PipelineOperation&&) noexcept = delete;

      public:
        virtual Result execute(const std::any& payload, const CancellationToken& token) = 0;
    };

    /// @brief
    /// Purpose: Runs owned CPU operations sequentially with an optional payload.
    /// @details
    /// Ownership: Owns operations through unique pointers.
    /// Thread Safety: Not thread-safe; synchronize mutation and execution externally.
    class TBX_API Pipeline
    {
      public:
        void add_operation(std::unique_ptr<PipelineOperation> operation);
        void clear_operations();
        const std::vector<std::unique_ptr<PipelineOperation>>& get_operations() const;

        Result execute(const std::any& payload, const CancellationToken& cancellation_token);
        Result execute(const std::any& payload, const CancellationToken& cancellation_token) const;
        Result execute(const std::any& payload) const;
        Result execute() const;

      private:
        std::vector<std::unique_ptr<PipelineOperation>> _operations = {};
    };
}
