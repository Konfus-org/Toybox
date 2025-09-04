#pragma once
#include "Tbx/DllExport.h"
#include <string>

#ifndef TBX_WORKING_ROOT_DIR
    #define TBX_WORKING_ROOT_DIR "./"
#endif

namespace Tbx::FileSystem
{
    EXPORT inline std::string GetWorkingDirectory()
    {
        return TBX_WORKING_ROOT_DIR;
    }
}