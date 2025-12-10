#include "tbx/file_system/json.h"
#include <nlohmann/json.hpp>

namespace tbx
{
    class Json::Impl
    {
      public:
        nlohmann::json Data;
    };

    Json::Json(const String& data)
        : _data(Scope<Impl>(new Impl()))
    {
        _data->Data = nlohmann::json::parse(data, nullptr, true, true);
    }

    Json::~Json() = default;

    String Json::to_string(int indent) const
    {
        return String(_data->Data.dump(indent));
    }

    bool Json::try_get_int(const String& key, int& out_value) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key.c_str());
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

        const auto iterator = _data->Data.find(key.c_str());
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_boolean())
            return false;

        out_value = iterator->get<bool>();
        return true;
    }

    bool Json::try_get_string(const String& key, String& out_value) const
    {
        if (!_data->Data.is_object())
            return false;

        const auto iterator = _data->Data.find(key.c_str());
        if (iterator == _data->Data.end())
            return false;

        if (!iterator->is_string())
            return false;

        out_value = String(iterator->get<std::string>());
        return true;
    }
}
