#include "PCH.h"
#include <Tbx/Core/Math/Mat4x4.h>

namespace Tbx::Tests
{
    TEST(Mat4x4Tests, MultiplyTest)
    {
        EXPECT_EQ(Tbx::Mat4x4(), Tbx::Mat4x4::Identity());
    }
}

