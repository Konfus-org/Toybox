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
    MaterialParameter::MaterialParameter(const std::string& parameter_name, TValue&& parameter_data)
        : MaterialParameter(std::string_view(parameter_name), std::forward<TValue>(parameter_data))
    {
    }

    template <typename TValue>
    MaterialParameter::MaterialParameter(const char* parameter_name, TValue&& parameter_data)
        : MaterialParameter(std::string_view(parameter_name), std::forward<TValue>(parameter_data))
    {
    }

    template <typename TValue>
    TValue MaterialParameterBindings::get_or(const std::string_view name, const TValue& fallback)
        const
    {
        const auto parameter = get(name);
        if (!parameter.has_value())
            return fallback;

        const auto& data = parameter->get().data;
        if (std::holds_alternative<TValue>(data))
            return std::get<TValue>(data);

        if constexpr (std::is_same_v<TValue, float>)
        {
            if (std::holds_alternative<double>(data))
                return static_cast<float>(std::get<double>(data));
            if (std::holds_alternative<int>(data))
                return static_cast<float>(std::get<int>(data));
        }
        else if constexpr (std::is_same_v<TValue, double>)
        {
            if (std::holds_alternative<float>(data))
                return static_cast<double>(std::get<float>(data));
            if (std::holds_alternative<int>(data))
                return static_cast<double>(std::get<int>(data));
        }
        else if constexpr (std::is_same_v<TValue, Vec4>)
        {
            if (std::holds_alternative<Vec3>(data))
                return Vec4(std::get<Vec3>(data), fallback.w);
            if (std::holds_alternative<Color>(data))
            {
                const auto& color = std::get<Color>(data);
                return Vec4(color.r, color.g, color.b, color.a);
            }
        }

        return fallback;
    }

}
