#include "tbx/systems/app/application.h"

int main()
{
    TBX_TRY_CATCH_ASSERT(
        {
            auto app = tbx::Application();

            // Run the application main loop
            return app.run();
        },
        "Application error occured!");
}
