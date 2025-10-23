#include "TIMSTextLoaderPlugin.h"
#include "Tbx/Debug/Asserts.h"
#include <stb_image.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>

namespace Tbx::Plugins::TIMS
{
    /////////////// LOADER /////////////////////

    bool TIMSTextLoaderPlugin::CanLoadText(const std::filesystem::path& filepath) const
    {
        return filepath.extension() == ".txt";
    }

    Ref<Text> TIMSTextLoaderPlugin::LoadText(const std::filesystem::path& filepath)
    {
        auto file = std::ifstream(filepath, std::ios::in | std::ios::binary);
        if (!file)
        {
            TBX_ASSERT(false, "TIMS: Failed to load text file at {}!", filepath.string());
            return {};
        }

        std::ostringstream contents;
        contents << file.rdbuf();
        file.close();

        return FactoryPlugin<TIMSText>::Create(contents.str(), "None", 12);
    }
}
