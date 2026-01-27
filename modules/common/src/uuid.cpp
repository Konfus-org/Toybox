#include "tbx/common/uuid.h"
#include "tbx/common/string_utils.h"
#include <charconv>
#include <cctype>
#include <random>
#include <sstream>
#include <string>

namespace tbx
{
    Uuid Uuid::generate()
    {
        Uuid id = {};

        std::random_device rd;
        std::mt19937_64 generator(rd());

        do
        {
            id.value = generator();
        } while (id.value == 0U);

        return id;
    }

    bool Uuid::is_valid() const
    {
        return value != 0U;
    }

    Uuid::operator bool() const
    {
        return is_valid();
    }

    bool Uuid::operator!() const
    {
        return !is_valid();
    }

    bool Uuid::operator<(const Uuid& other) const
    {
        return value < other.value;
    }

    bool Uuid::operator>(const Uuid& other) const
    {
        return value > other.value;
    }

    bool Uuid::operator<=(const Uuid& other) const
    {
        return value <= other.value;
    }

    bool Uuid::operator>=(const Uuid& other) const
    {
        return value >= other.value;
    }

    Uuid::operator uint32() const
    {
        return value;
    }

    bool Uuid::operator==(const Uuid& other) const
    {
        return value == other.value;
    }

    bool Uuid::operator!=(const Uuid& other) const
    {
        return !(value == other.value);
    }

    std::string to_string(const Uuid& value)
    {
        std::ostringstream stream = {};
        stream << std::hex << value.value;
        return stream.str();
    }

    Uuid from_string(std::string_view value)
    {
        const std::string trimmed = trim(value);
        if (trimmed.empty())
        {
            return {};
        }
        auto start = trimmed.data();
        auto end = trimmed.data() + trimmed.size();
        while (start < end && !std::isxdigit(static_cast<unsigned char>(*start)))
        {
            start += 1;
        }
        if (start == end)
        {
            return {};
        }
        auto token_end = start;
        while (token_end < end && std::isxdigit(static_cast<unsigned char>(*token_end)))
        {
            token_end += 1;
        }
        uint32 parsed = 0U;
        auto result = std::from_chars(start, token_end, parsed, 16);
        if (result.ec != std::errc())
        {
            return {};
        }
        if (parsed == 0U)
        {
            return {};
        }
        return Uuid(parsed);
    }

}
