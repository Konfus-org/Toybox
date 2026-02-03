#include "pch.h"
#include "tbx/messages/message.h"

namespace tbx::tests::messaging
{
    TEST(MessageTests, InitializesWithDefaults)
    {
        Message message = {};

        EXPECT_EQ(message.state, MessageState::UnHandled);
        EXPECT_TRUE(message.result.succeeded());
        EXPECT_EQ(message.id, message.id);
    }

    TEST(MessageTests, RequestsDefaultToDoNothingWhenUnhandled)
    {
        Request<void> request = {};

        EXPECT_EQ(request.not_handled_behavior, MessageNotHandledBehavior::DoNothing);
        EXPECT_EQ(request.state, MessageState::UnHandled);
    }
}
