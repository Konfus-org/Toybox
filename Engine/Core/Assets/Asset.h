#pragma once
#include "tbxpch.h"
#include "tbxAPI.h"

namespace Toybox
{
    class Asset
    {
    public:
        TBX_API Asset() = default;
        TBX_API virtual ~Asset() = default;

        TBX_API const std::string& GetName() const { return _name; }
        TBX_API void SetName(const std::string_view& name) { _name = name; }

        TBX_API bool Load(const std::string& filePath);
        TBX_API void Unload();

    protected:
        virtual bool LoadData(const std::filebuf& fileContents) = 0;

    private:
        std::string _name;
        std::string _sourceFilePath;
    };
}