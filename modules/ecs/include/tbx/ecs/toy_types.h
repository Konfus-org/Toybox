#pragma once
#include "tbx/common/uuid.h"
#include <any>
#include <string>
#include <vector>

namespace tbx
{
    using Sticker = std::string;
    using Block = std::any;

    struct Toy
    {
        std::string name = "";
        std::vector<Sticker> stickers = {};
        Uuid parent = invalid::uuid;
        Uuid id = Uuid::generate();
    };

    namespace invalid
    {
        inline Block block = std::any();
        inline Toy toy = Toy("INVALID", {}, invalid::uuid, invalid::uuid);
    }
}
