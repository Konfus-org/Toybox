#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/TBS/PlaySpace.h"
#include <memory>

namespace Tbx
{
    class World
    {
    public:
        EXPORT static void Initialize();
        EXPORT static void Update();
        EXPORT static void Destroy();

        EXPORT static UID MakePlayspace();
        EXPORT static void RemovePlayspace(UID id);

        EXPORT static std::shared_ptr<PlaySpace> GetPlayspace(UID id);
        EXPORT static std::vector<std::shared_ptr<PlaySpace>> GetPlaySpaces();
        EXPORT static uint32 GetPlayspaceCount();

    private:
        // TODO: make a pool of playSpaces
        static std::vector<std::shared_ptr<PlaySpace>> _playSpaces;
        static uint32 _playSpaceCount;
    };
}