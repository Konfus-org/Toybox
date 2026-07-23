#include "tbx/serialization/registry.h"
#include <algorithm>

namespace tbx
{
    SerializerInfo& SerializerRegistry::add(SerializerInfo info)
    {
        const auto existing = std::ranges::find_if(
            _serializers,
            [&info](const std::unique_ptr<SerializerInfo>& serializer)
            { return serializer->type_hash == info.type_hash; });
        if (existing != _serializers.end())
            return **existing;
        _serializers.push_back(std::make_unique<SerializerInfo>(std::move(info)));
        return *_serializers.back();
    }

    void SerializerRegistry::clear()
    {
        _serializers.clear();
    }

    std::vector<std::reference_wrapper<const SerializerInfo>> SerializerRegistry::get_all() const
    {
        auto all = std::vector<std::reference_wrapper<const SerializerInfo>>();
        all.reserve(_serializers.size());
        for (const std::unique_ptr<SerializerInfo>& serializer : _serializers)
            all.push_back(std::cref(*serializer));
        return all;
    }

    std::optional<std::reference_wrapper<const SerializerInfo>> SerializerRegistry::find(
        const size type_hash) const
    {
        for (const std::unique_ptr<SerializerInfo>& serializer : _serializers)
            if (serializer->type_hash == type_hash)
                return std::cref(*serializer);
        return {};
    }

    std::optional<std::reference_wrapper<const SerializerInfo>> SerializerRegistry::find_by_extension(
        const std::string_view extension) const
    {
        for (const std::unique_ptr<SerializerInfo>& serializer : _serializers)
            for (const std::string& ext : serializer->extensions)
                if (ext == extension)
                    return std::cref(*serializer);
        return {};
    }

    SerializerRegistry& get_serializer_registry()
    {
        static SerializerRegistry g_registry;
        return g_registry;
    }

    std::optional<std::reference_wrapper<const SerializerInfo>> find_serializer_by_extension(
        const std::string_view extension)
    {
        return get_serializer_registry().find_by_extension(extension);
    }
}
