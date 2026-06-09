#pragma once

namespace tbx
{
    template <typename TValue>
    TValue MaterialInstance::get_parameter_or(const std::string& name, const TValue& fallback) const
    {
        const auto parameter = overrides.parameters.get(name);
        if (!parameter.has_value())
            return fallback;

        if (std::holds_alternative<TValue>(parameter->get().data))
            return std::get<TValue>(parameter->get().data);

        return fallback;
    }
}
