#pragma once

namespace tbx
{
    template <typename TValue>
    TValue MaterialInstance::get_parameter_or(const std::string& name, const TValue& fallback) const
    {
        for (const auto& parameter : overrides.parameters)
        {
            if (parameter.name != name)
                continue;

            if (std::holds_alternative<TValue>(parameter.data))
                return std::get<TValue>(parameter.data);

            return fallback;
        }

        return fallback;
    }
}
