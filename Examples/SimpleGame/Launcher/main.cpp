#include "main.h"
#include <Tbx/Launcher/Launcher.h>

int main()
{
    auto status = Tbx::Launch();
    if (status == Tbx::AppStatus::Error)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
