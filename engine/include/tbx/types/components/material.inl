#pragma once

namespace tbx
{
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
