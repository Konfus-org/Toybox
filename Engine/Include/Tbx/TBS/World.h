#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/TBS/PlaySpace.h"
#include <memory>

namespace Tbx
{
    class World
    {
    public:
        EXPORT static void SetContext();
        EXPORT static void Update();
        EXPORT static void Destroy();

        EXPORT static UID MakePlayspace();
        EXPORT static void RemovePlayspace(UID id);

        EXPORT static std::shared_ptr<Playspace> GetPlayspace(UID id);
        EXPORT static std::vector<std::shared_ptr<Playspace>> GetPlayspaces();
        EXPORT static uint32 GetPlayspaceCount();

    private:
        // TODO: make a pool of playSpaces
        static std::vector<std::shared_ptr<Playspace>> _playSpaces;
        static uint32 _playSpaceCount;
    };
}