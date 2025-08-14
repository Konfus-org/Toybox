#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/TBS/Box.h"
#include <memory>

namespace Tbx
{
    class World
    {
    public:
        EXPORT static void SetContext();
        EXPORT static void Update();
        EXPORT static void Destroy();

        EXPORT static Uid MakeBox();
        EXPORT static void RemoveBox(Uid id);

        EXPORT static std::shared_ptr<Box> GetBox(Uid id);
        EXPORT static std::vector<std::shared_ptr<Box>> GetBoxes();
        EXPORT static uint32 GetBoxCount();

    private:
        // TODO: make a pool of boxes
        static std::vector<std::shared_ptr<Box>> _boxes;
        static uint32 _boxCount;
    };
}