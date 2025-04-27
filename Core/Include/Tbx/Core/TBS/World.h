#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/TBS/Playspace.h"
#include <memory>
#include <vector>

namespace Tbx
{
    class World
    {
    public:
        EXPORT static std::shared_ptr<Playspace> AddPlayspace();
        EXPORT static void AddPlayspace(std::shared_ptr<Playspace> playspace);
        EXPORT static void RemovePlayspace(std::shared_ptr<Playspace> playspace);
        EXPORT static std::vector<std::shared_ptr<Playspace>> GetPlayspaces();

        EXPORT static void SetMainPlayspace(std::shared_ptr<Playspace> playspace);
        EXPORT static std::shared_ptr<Playspace> GetMainPlayspace();


    private:
        static std::shared_ptr<Playspace> _mainPlayspace; 
        static std::vector<std::shared_ptr<Playspace>> _playspaces;
    };
}