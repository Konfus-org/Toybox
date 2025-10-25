#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    struct ApplicationDescription
    {
        std::string name = "";
        std::filesystem::path working_root = "";
        std::filesystem::path assets_directory = "";
        std::filesystem::path plugins_directory = "";
    };

    class Application
    {
    public:
        Application(const ApplicationDescription& desc);

        // Starts the application main loop. Returns process exit code.
        int run();
    };
}

