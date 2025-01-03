#include "TbxPCH.h"
#include "Asset.h"
#include "Debug/Debugging.h"

namespace Tbx
{
    bool Asset::Load(const std::string& filePath)
    {
        _sourceFilePath = filePath;
        _name = filePath.substr(filePath.find_last_of("/\\") + 1); 
        ////std::ifstream sourceFile(filePath);
        ////if (!sourceFile)
        ////{
        ////    TBX_ERROR("Failed to load asset from file {0}!", filePath);
        ////    return false;
        ////}
        return true;
    }

    void Asset::Unload()
    {
    }
}