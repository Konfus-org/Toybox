#pragma once
#include "tbx/tbx_api.h"
#include <any>
#include <memory>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Defines a unit of work that can be executed by a pipeline.
    /// @details
    /// Ownership: Pipeline operations own their internal state and are held by unique ownership.
    /// Thread Safety: Not thread-safe; callers must synchronize access.

    class TBX_API PipelineOperation
    {
      public:
        virtual ~PipelineOperation() = default;

        /// @brief
        /// Purpose: Executes the operation logic using an optional execution payload.
        /// @details
        /// Ownership: Does not transfer ownership of referenced resources.
        /// Thread Safety: Not thread-safe; call from the owning thread.

        virtual void execute(const std::any& payload) = 0;

        /// @brief
        /// Purpose: Executes the operation logic with an empty payload.
        /// @details
        /// Ownership: Does not transfer ownership of referenced resources.
        /// Thread Safety: Not thread-safe; call from the owning thread.

        void execute()
        {
            execute(std::any {});
        }
    };

    /// @brief
    /// Purpose: Runs a sequence of pipeline operations in order.
    /// @details
    /// Ownership: Owns the operations via unique pointers.
    /// Thread Safety: Not thread-safe; synchronize when used across threads.

    class TBX_API Pipeline : public PipelineOperation
    {
      public:
        Pipeline() = default;
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        Pipeline(Pipeline&&) noexcept = default;
        Pipeline& operator=(Pipeline&&) noexcept = default;

        /// @brief
        /// Purpose: Adds a new operation to the end of the pipeline.
        /// @details
        /// Ownership: Takes unique ownership of the operation.
        /// Thread Safety: Not thread-safe; callers must synchronize.

        void add_operation(std::unique_ptr<PipelineOperation> operation);

        /// @brief
        /// Purpose: Removes all operations from the pipeline.
        /// @details
        /// Ownership: Releases unique ownership of stored operations.
        /// Thread Safety: Not thread-safe; callers must synchronize.

        void clear_operations();

        /// @brief
        /// Purpose: Returns the currently configured operations.
        /// @details
        /// Ownership: Returns a non-owning reference to the stored operations.
        /// Thread Safety: Not thread-safe; callers must synchronize.

        const std::vector<std::unique_ptr<PipelineOperation>>& get_operations() const;

        /// @brief
        /// Purpose: Executes each operation in sequence with the given payload.
        /// @details
        /// Ownership: Does not transfer ownership of operations or payload resources.
        /// Thread Safety: Not thread-safe; call from the owning thread.

        void execute(const std::any& payload) override;

        /// @brief
        /// Purpose: Executes each operation in sequence with an empty payload.
        /// @details
        /// Ownership: Does not transfer ownership of operations.
        /// Thread Safety: Not thread-safe; call from the owning thread.

        void execute();

      private:
        std::vector<std::unique_ptr<PipelineOperation>> _operations = {};
    };
}
