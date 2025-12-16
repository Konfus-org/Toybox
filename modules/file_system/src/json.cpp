#include "tbx/file_system/json.h"
#include <nlohmann/json.hpp>

namespace tbx
{
    class Json::Impl
    {
      public:
        nlohmann::json Data;
    };

    Json::Json()
        : _data(Scope<Impl>(new Impl()))
    {
    }

    Json::Json(const String& data)
        : _data(Scope<Impl>(new Impl()))
    {
        _data->Data = nlohmann::json::parse(data, nullptr, true, true);
    }

    Json::Json(Json&& other) = default;

    Json& Json::operator=(Json&& other) = default;

    Json::~Json() = default;

    String Json::to_string(int indent) const
    {
        return String(_data->Data.dump(indent));
    }

    bool Json::try_get_int(const String& key, int& out_value) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key.get_cstr());
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_number_integer())
            return false;

        out_value = iterator->get<int>();
        return true;
    }

    bool Json::try_get_bool(const String& key, bool& out_value) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key.get_cstr());
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_boolean())
            return false;

        out_value = iterator->get<bool>();
        return true;
    }

    bool Json::try_get_float(const String& key, double& out_value) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key.get_cstr());
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_number())
            return false;

        out_value = iterator->get<double>();
        return true;
    }

    bool Json::try_get_string(const String& key, String& out_value) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key.get_cstr());
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_string())
            return false;

        out_value = String(iterator->get<std::string>());
        return true;
    }

    bool Json::try_get_strings(const String& key, List<String>& out_values) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key.get_cstr());
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_array())
            return false;

        bool found = false;
        for (const auto& entry : *iterator)
        {
            if (entry.is_string())
            {
                out_values.push_back(String(entry.get<std::string>()));
                found = true;
            }
        }

        return found;
    }

    bool Json::try_get_ints(const String& key, List<int>& out_values) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key.get_cstr());
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_array())
            return false;

        bool found = false;
        for (const auto& entry : *iterator)
        {
            if (entry.is_number_integer())
            {
                out_values.push_back(entry.get<int>());
                found = true;
            }
        }

        return found;
    }

    bool Json::try_get_bools(const String& key, List<bool>& out_values) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key.get_cstr());
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_array())
            return false;

        bool found = false;
        for (const auto& entry : *iterator)
        {
            if (entry.is_boolean())
            {
                out_values.push_back(entry.get<bool>());
                found = true;
            }
        }

        return found;
    }

    bool Json::try_get_floats(const String& key, List<double>& out_values) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key.get_cstr());
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_array())
            return false;

        bool found = false;
        for (const auto& entry : *iterator)
        {
            if (entry.is_number())
            {
                out_values.push_back(entry.get<double>());
                found = true;
            }
        }

        return found;
    }

    bool Json::try_get_child(const String& key, Json& out_value) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key.get_cstr());
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_object())
            return false;

        out_value._data = Scope<Impl>(new Impl());
        out_value._data->Data = *iterator;
        return true;
    }

    bool Json::try_get_children(const String& key, List<Json>& out_values) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key.get_cstr());
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_array())
            return false;

        bool found = false;
        for (const auto& entry : *iterator)
        {
            if (entry.is_object())
            {
                Json child;
                child._data->Data = entry;
                out_values.push_back(std::move(child));
                found = true;
            }
        }

        return found;
    }
}
