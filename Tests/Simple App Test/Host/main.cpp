#include "main.h"
#include <Tbx/RuntimeHost/Host.h>

int main()
{
    auto status = Tbx::RunHost(PATH_TO_PLUGINS);
    if (status == Tbx::AppStatus::Error)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}