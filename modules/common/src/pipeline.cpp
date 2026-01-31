#include "tbx/common/pipeline.h"

namespace tbx
{
    void Pipeline::add_operation(std::unique_ptr<PipelineOperation> operation)
    {
        if (!operation)
        {
            return;
        }

        _operations.push_back(std::move(operation));
    }

    void Pipeline::clear_operations()
    {
        _operations.clear();
    }

    const std::vector<std::unique_ptr<PipelineOperation>>& Pipeline::get_operations() const
    {
        return _operations;
    }

    void Pipeline::execute()
    {
        for (const auto& operation : _operations)
        {
            if (!operation)
            {
                continue;
            }

            operation->execute();
        }
    }
}
