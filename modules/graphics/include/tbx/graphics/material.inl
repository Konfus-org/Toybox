#pragma once

namespace tbx
{
    template <typename TValue>
    MaterialParameter::MaterialParameter(std::string_view parameter_name, TValue&& parameter_data)
        : name(parameter_name)
        , data(std::forward<TValue>(parameter_data))
    {
    }

    template <typename TValue>
    TValue MaterialInstance::get_parameter_or(std::string_view name, const TValue& fallback) const
    {
        const auto* parameter = param_overrides.get(name);
        if (!parameter)
            return fallback;

        if (const auto* value = std::get_if<TValue>(&parameter->data))
            return *value;

        return fallback;
    }
}
