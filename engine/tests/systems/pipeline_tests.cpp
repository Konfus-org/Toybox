#include "pch.h"
#include "tbx/systems/pipeline.h"
#include "tbx/types/typedefs.h"
#include <any>
#include <memory>

namespace tbx::tests::common
{
    class RecordingOperation final : public PipelineOperation
    {
      public:
        RecordingOperation(uint32& call_count, Result result = {})
            : _call_count(&call_count)
            , _result(result)
        {
        }

      public:
        Result execute(const std::any&, const CancellationToken& cancellation_token) override
        {
            if (cancellation_token && cancellation_token.is_cancelled())
                return Result(false, "Cancelled.");

            ++(*_call_count);
            return _result;
        }

      private:
        uint32* _call_count = nullptr;
        Result _result = {};
    };

    TEST(PipelineTests, Execute_RunsOperationsInOrder)
    {
        // Arrange
        auto first_call_count = 0U;
        auto second_call_count = 0U;
        auto pipeline = Pipeline {};
        pipeline.add_operation(std::make_unique<RecordingOperation>(first_call_count));
        pipeline.add_operation(std::make_unique<RecordingOperation>(second_call_count));

        // Act
        const auto result = pipeline.execute();

        // Assert
        EXPECT_TRUE(result);
        EXPECT_EQ(first_call_count, 1U);
        EXPECT_EQ(second_call_count, 1U);
    }

    TEST(PipelineTests, Execute_StopsWhenOperationFails)
    {
        // Arrange
        auto first_call_count = 0U;
        auto second_call_count = 0U;
        auto pipeline = Pipeline {};
        pipeline.add_operation(
            std::make_unique<RecordingOperation>(first_call_count, Result(false, "First failed.")));
        pipeline.add_operation(std::make_unique<RecordingOperation>(second_call_count));

        // Act
        const auto result = pipeline.execute();

        // Assert
        EXPECT_FALSE(result);
        EXPECT_EQ(result.get_report(), "First failed.");
        EXPECT_EQ(first_call_count, 1U);
        EXPECT_EQ(second_call_count, 0U);
    }

    TEST(PipelineTests, Execute_StopsWhenCancelled)
    {
        // Arrange
        auto call_count = 0U;
        auto cancellation_source = CancellationSource {};
        auto pipeline = Pipeline {};
        pipeline.add_operation(std::make_unique<RecordingOperation>(call_count));
        cancellation_source.cancel();

        // Act
        const auto result = pipeline.execute(std::any {}, cancellation_source.get_token());

        // Assert
        EXPECT_FALSE(result);
        EXPECT_EQ(result.get_report(), "Pipeline execution cancelled.");
        EXPECT_EQ(call_count, 0U);
    }
}
