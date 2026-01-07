#include "pch.h"
#include "tbx/messages/message.h"

namespace tbx::tests::messaging
{
    TEST(MessageTests, InitializesWithDefaults)
    {
        Message message = {};

        EXPECT_EQ(message.state, MessageState::UnHandled);
        EXPECT_FALSE(message.require_handling);
        EXPECT_FALSE(message.result.succeeded());
        EXPECT_EQ(message.id, message.id);
    }
}
