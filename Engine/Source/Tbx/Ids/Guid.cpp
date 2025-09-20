#include "Tbx/PCH.h"
#include "Tbx/Ids/Guid.h"

#include <random>

namespace Tbx
{
    Guid Guid::Invalid = Guid("00000000-0000-0000-0000-000000000000");

    Guid Guid::Generate()
    {
        static thread_local std::mt19937_64 engine(std::random_device{}());
        static constexpr char HexDigits[] = "0123456789abcdef";
        std::uniform_int_distribution<int> distribution(0, 15);

        std::string value(36, '\0');
        value[8] = value[13] = value[18] = value[23] = '-';

        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (value[index] == '-')
            {
                continue;
            }

            value[index] = HexDigits[distribution(engine)];
        }

        // Version field (UUID version 4)
        value[14] = '4';

        // Variant field (UUID variant 1)
        int variant = distribution(engine) & 0x3;
        value[19] = HexDigits[variant | 0x8];

        return Guid(std::move(value));
    }
}