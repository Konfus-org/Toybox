#pragma once

namespace tbx
{
    template <typename TValue>
    MaterialParameter::MaterialParameter(std::string_view parameter_name, TValue&& parameter_data)
        : name(parameter_name)
        , data(std::forward<TValue>(parameter_data))
    {
    }

}
