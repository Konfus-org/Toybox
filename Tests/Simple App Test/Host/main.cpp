#include "main.h"
#include <Tbx/Runtime/Runtime.h>
#include <Tbx/Loader/Loader.h>

Tbx::AppStatus RunApp()
{
    auto app = Tbx::Load(PATH_TO_PLUGINS);
    auto status = Tbx::Run(app);

    if (status == Tbx::AppStatus::Reloading)
    {
        status = RunApp();
    }

    return status;
}

int main()
{
    auto status = RunApp();
    if (status == Tbx::AppStatus::Error)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}