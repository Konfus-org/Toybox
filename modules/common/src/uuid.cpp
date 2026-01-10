#include "tbx/common/uuid.h"
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

}
