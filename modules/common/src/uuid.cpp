#include "tbx/common/uuid.h"
#include <functional>
#include <random>
#include <sstream>
#include <string>

namespace tbx
{
    const Uuid Uuid::NONE = Uuid();

    Uuid::Uuid() = default;
    Uuid::Uuid(uint32 v)
        : value(v)
    {
    }

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

    static uint32 combine_value(uint32 seed, uint32 value)
    {
        auto hashed = std::hash<uint32> {}(value);
        seed ^= hashed + 0x9e3779b9U + (seed << 6) + (seed >> 2);
        return seed;
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
        {
            this->value = 1U;
        }
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
}
