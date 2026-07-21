#include "pch.h"
#include "test_runtimes.h"

namespace tbx::tests
{
    // Mirrors a production host: engine types are registered explicitly before anything runs
    // (the launcher does this at startup; the test binary does it here).
    class EngineTypeRegistrationEnvironment final : public ::testing::Environment
    {
      public:
        void SetUp() override
        {
            ensure_engine_types_registered();
        }
    };

    static const bool engine_types_environment_registered = []
    {
        ::testing::AddGlobalTestEnvironment(new EngineTypeRegistrationEnvironment());
        return true;
    }();
}
