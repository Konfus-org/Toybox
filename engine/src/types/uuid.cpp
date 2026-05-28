#include "tbx/types/uuid.h"
#include <charconv>
#include <random>
#include <sstream>

namespace tbx
{
    static uint32 combine_value(uint32 seed, uint32 value)
    {
        auto hashed = std::hash<uint32> {}(value);
        seed ^= hashed + 0x9e3779b9U + (seed << 6) + (seed >> 2);
        return seed;
    }

    Uuid::Uuid() = default;
    Uuid::Uuid(uint32 v)
        : value(v)
    {
    }

    Uuid Uuid::generate()
    {
        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_int_distribution<uint32> dist(1u, std::numeric_limits<uint32>::max());

        Uuid id = dist(generator);

        return id;
    }

    Uuid Uuid::combine(Uuid base, uint32 value)
    {
        base.combine(value);
        return base;
    }

    void Uuid::combine(uint32 value)
    {
        this->value = combine_value(this->value, value);
        if (this->value == 0U)
            this->value = 1U;
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

    Uuid parse_uuid(std::string_view value)
    {
        auto trimmed = value;
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front())) != 0)
            trimmed.remove_prefix(1U);
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back())) != 0)
            trimmed.remove_suffix(1U);
        if (trimmed.empty())
            return {};

        if (trimmed.size() > 2U && trimmed[0] == '0' && (trimmed[1] == 'x' || trimmed[1] == 'X'))
            trimmed.remove_prefix(2U);

        auto parsed = uint32();
        const auto* begin = trimmed.data();
        const auto* end = trimmed.data() + trimmed.size();
        const auto result = std::from_chars(begin, end, parsed, 16);
        if (result.ec != std::errc() || result.ptr != end || parsed == 0U)
            return {};

        return Uuid(parsed);
    }

    Uuid hash_string_to_id(std::string_view handle_name)
    {
        const auto hasher = std::hash<std::string_view>();
        const auto hashed = static_cast<uint32>(hasher(handle_name));
        return hashed == 0U ? Uuid(1U) : Uuid(hashed);
    }
}
