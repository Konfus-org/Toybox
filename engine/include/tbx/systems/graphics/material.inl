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
        const auto parameter = param_overrides.get(name);
        if (!parameter.has_value())
            return fallback;

        if (std::holds_alternative<TValue>(parameter->get().data))
            return std::get<TValue>(parameter->get().data);

        return fallback;
    }
}
