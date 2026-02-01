#pragma once
#include "tbx/tbx_api.h"
#include <memory>
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Defines a unit of work that can be executed by a pipeline.
    /// </summary>
    /// <remarks>
    /// Ownership: Pipeline operations own their internal state and are held by unique ownership.
    /// Thread Safety: Not thread-safe; callers must synchronize access.
    /// </remarks>
    class TBX_API PipelineOperation
    {
      public:
        virtual ~PipelineOperation() = default;

        /// <summary>
        /// Purpose: Executes the operation logic.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership of referenced resources.
        /// Thread Safety: Not thread-safe; call from the owning thread.
        /// </remarks>
        virtual void execute() = 0;
    };

    /// <summary>
    /// Purpose: Runs a sequence of pipeline operations in order.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the operations via unique pointers.
    /// Thread Safety: Not thread-safe; synchronize when used across threads.
    /// </remarks>
    class TBX_API Pipeline final : public PipelineOperation
    {
      public:
        /// <summary>
        /// Purpose: Constructs an empty pipeline.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of external resources.
        /// Thread Safety: Not thread-safe; construct on the owning thread.
        /// </remarks>
        Pipeline() = default;

        /// <summary>
        /// Purpose: Prevents copying of pipeline instances.
        /// </summary>
        /// <remarks>
        /// Ownership: Copying is disallowed because operations are uniquely owned.
        /// Thread Safety: Not thread-safe; copy operations are not supported.
        /// </remarks>
        Pipeline(const Pipeline&) = delete;

        /// <summary>
        /// Purpose: Prevents copy assignment between pipelines.
        /// </summary>
        /// <remarks>
        /// Ownership: Copy assignment is disallowed because operations are uniquely owned.
        /// Thread Safety: Not thread-safe; copy operations are not supported.
        /// </remarks>
        Pipeline& operator=(const Pipeline&) = delete;

        /// <summary>
        /// Purpose: Transfers ownership of operations from another pipeline.
        /// </summary>
        /// <remarks>
        /// Ownership: Takes ownership of the moved-from pipeline's operations.
        /// Thread Safety: Not thread-safe; move on the owning thread.
        /// </remarks>
        Pipeline(Pipeline&&) noexcept = default;

        /// <summary>
        /// Purpose: Replaces this pipeline with another via move.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases current operations and takes ownership from the source pipeline.
        /// Thread Safety: Not thread-safe; move on the owning thread.
        /// </remarks>
        Pipeline& operator=(Pipeline&&) noexcept = default;

        /// <summary>
        /// Purpose: Adds a new operation to the end of the pipeline.
        /// </summary>
        /// <remarks>
        /// Ownership: Takes unique ownership of the operation.
        /// Thread Safety: Not thread-safe; callers must synchronize.
        /// </remarks>
        void add_operation(std::unique_ptr<PipelineOperation> operation);

        /// <summary>
        /// Purpose: Removes all operations from the pipeline.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases unique ownership of stored operations.
        /// Thread Safety: Not thread-safe; callers must synchronize.
        /// </remarks>
        void clear_operations();

        /// <summary>
        /// Purpose: Returns the currently configured operations.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a non-owning reference to the stored operations.
        /// Thread Safety: Not thread-safe; callers must synchronize.
        /// </remarks>
        const std::vector<std::unique_ptr<PipelineOperation>>& get_operations() const;

        /// <summary>
        /// Purpose: Executes each operation in sequence.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership of operations.
        /// Thread Safety: Not thread-safe; call from the owning thread.
        /// </remarks>
        void execute() override;

      private:
        std::vector<std::unique_ptr<PipelineOperation>> _operations = {};
    };
}
