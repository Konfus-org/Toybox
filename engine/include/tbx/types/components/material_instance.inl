#pragma once

namespace tbx
{
    template <typename TValue>
    TValue MaterialInstance::get_parameter_or(const std::string& name, const TValue& fallback) const
    {
        return get_parameter_or(make_param_id(name), fallback);
    }

    template <typename TValue>
    TValue MaterialInstance::get_parameter_or(const uint32 id, const TValue& fallback) const
    {
        const auto parameter = overrides.parameters.get(id);
        if (!parameter.has_value())
            return fallback;

        if (std::holds_alternative<TValue>(parameter->get().data))
            return std::get<TValue>(parameter->get().data);

        return fallback;
    }
}
