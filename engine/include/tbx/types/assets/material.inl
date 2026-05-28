#pragma once

namespace tbx
{
    template <typename TValue>
    MaterialParameter::MaterialParameter(std::string_view parameter_name, TValue&& parameter_data)
        : name(parameter_name)
        , id(make_param_id(parameter_name))
        , data(std::forward<TValue>(parameter_data))
    {
    }

    template <typename TValue>
    MaterialParameter::MaterialParameter(const std::string& parameter_name, TValue&& parameter_data)
        : MaterialParameter(std::string_view(parameter_name), std::forward<TValue>(parameter_data))
    {
    }

}
