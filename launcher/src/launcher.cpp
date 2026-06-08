#include "launcher.h"
#include "tbx/systems/app/application.h"
#include "tbx/systems/app/command_list.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/plugin_api/shared_library.h"
#include <filesystem>
#include <memory>

int Launcher::run(int argc, char* argv[])
{
    auto exit_code = -1;

    try
    {
        const auto command_list = tbx::CommandList(argc, argv);
        const auto executable_directory = tbx::get_process_executable_directory();
        const auto app_module_name = command_list.get<std::string>("app");
        if (app_module_name.empty())
        {
            TBX_ASSERT(false, "Launcher requires an '--app=<AppModuleName>' argument.");
            return -1;
        }

        const auto working_directory_value = command_list.get<std::string>("working-dir");
        const auto working_directory =
            working_directory_value.empty()
                ? executable_directory.lexically_normal()
                : std::filesystem::path(working_directory_value).lexically_normal();

        auto app_library = tbx::load_shared_lib(executable_directory / app_module_name);
        if (!app_library->is_valid())
        {
            auto load_error_message = std::string {};
            const auto app_library_path = app_library->get_path();
            if (app_library->try_get_load_error_message(load_error_message))
            {
                TBX_ASSERT(
                    false,
                    "Launcher failed to load app module '{}': {}.",
                    app_library_path.string(),
                    load_error_message);
            }
            else
            {
                TBX_ASSERT(
                    false,
                    "Launcher failed to load app module '{}'.",
                    app_library_path.string());
            }
            return -1;
        }

        auto create_app = app_library->get_symbol<tbx::CreateAppFn>("tbx_create_app");
        auto destroy_app = app_library->get_symbol<tbx::DestroyAppFn>("tbx_destroy_app");
        if (create_app == nullptr || destroy_app == nullptr)
        {
            TBX_ASSERT(false, "Launcher found an invalid app module.");
            return -1;
        }

        auto app = std::unique_ptr<tbx::Application, tbx::DestroyAppFn>(create_app(), destroy_app);
        if (!app)
        {
            TBX_ASSERT(false, "Launcher failed to construct the app.");
            return -1;
        }

        exit_code = app->run(command_list, working_directory);
        app.reset();
    }
    catch (const std::exception& ex)
    {
        TBX_ASSERT(false, "Exception during launcher run: {}", ex.what());
        exit_code = -1;
    }
    catch (...)
    {
        TBX_ASSERT(false, "Unknown exception during launcher run.");
        exit_code = -1;
    }
    return exit_code;
}
