#include "pch.h"
#include "tbx/messages/message.h"

namespace tbx::tests::messaging
{
    TEST(MessageTests, InitializesWithDefaults)
    {
        Message message = {};

        EXPECT_EQ(message.state, MessageState::UnHandled);
        EXPECT_TRUE(message.payload.has_value() == false);
        EXPECT_EQ(message.id, message.id);
    }
}
