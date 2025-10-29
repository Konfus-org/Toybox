#include "pch.h"
#include "tbx/state/cancellation_token.h"

namespace tbx::tests::state
{
    TEST(cancellation_token, default_is_not_cancelled)
    {
        ::tbx::CancellationToken token;
        EXPECT_FALSE(token);
        EXPECT_FALSE(token.is_cancelled());
    }

    TEST(cancellation_token, reflects_source_cancellation)
    {
        ::tbx::CancellationSource source;
        auto token = source.token();

        EXPECT_TRUE(token);
        EXPECT_FALSE(token.is_cancelled());

        source.cancel();

        EXPECT_TRUE(token.is_cancelled());
        EXPECT_TRUE(source.is_cancelled());
    }

    TEST(cancellation_token, shares_state_between_tokens)
    {
        ::tbx::CancellationSource source;
        auto first = source.token();
        auto second = source.token();

        source.cancel();

        EXPECT_TRUE(first.is_cancelled());
        EXPECT_TRUE(second.is_cancelled());

        auto late = source.token();
        EXPECT_TRUE(late.is_cancelled());
    }
}

