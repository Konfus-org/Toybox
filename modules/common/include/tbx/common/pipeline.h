#pragma once
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
    class PipelineOperation
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
    class Pipeline final : public PipelineOperation
    {
      public:
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
