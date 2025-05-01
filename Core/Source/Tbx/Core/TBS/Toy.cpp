#include "Tbx/Core/PCH.h"
#include "Tbx/Core/TBS/Toy.h"

namespace Tbx
{
    std::unordered_map<hash, uint32> BlockTypeIndexProvider::_blockTypeTable = {};

    std::unordered_map<hash, uint32>& BlockTypeIndexProvider::GetMap()
    {
        return _blockTypeTable;
    }
}
