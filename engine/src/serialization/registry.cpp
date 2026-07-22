#include "tbx/serialization/registry.h"
#include <algorithm>

namespace tbx::serialization
{
    Info& Registry::add(Info info)
    {
        const auto existing = std::ranges::find_if(
            _serializers,
            [&info](const std::unique_ptr<Info>& serializer)
            { return serializer->type_hash == info.type_hash; });
        if (existing != _serializers.end())
            return **existing;
        _serializers.push_back(std::make_unique<Info>(std::move(info)));
        return *_serializers.back();
    }

    std::vector<std::reference_wrapper<const Info>> Registry::get_all() const
    {
        auto all = std::vector<std::reference_wrapper<const Info>>();
        all.reserve(_serializers.size());
        for (const std::unique_ptr<Info>& serializer : _serializers)
            all.push_back(std::cref(*serializer));
        return all;
    }

    std::optional<std::reference_wrapper<const Info>> Registry::find(
        const size type_hash) const
    {
        for (const std::unique_ptr<Info>& serializer : _serializers)
            if (serializer->type_hash == type_hash)
                return std::cref(*serializer);
        return {};
    }

    Registry& get_serializer_registry()
    {
        static Registry g_registry;
        return g_registry;
    }
}
