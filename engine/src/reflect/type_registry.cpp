#include "tbx/reflect/type_info.h"
#include "tbx/core/log.h"

namespace tbx
{
    //// TYPE REGISTRY ////

    TypeInfo& TypeRegistry::add(TypeInfo info)
    {
        for (auto& existing : _types)
        {
            if (existing->name_hash == info.name_hash)
            {
                log_warn("type '{}' re-registered; replacing its previous record", info.name);
                *existing = std::move(info);
                return *existing;
            }
        }
        _types.push_back(std::make_unique<TypeInfo>(std::move(info)));
        return *_types.back();
    }

    std::vector<std::reference_wrapper<const TypeInfo>> TypeRegistry::get_all() const
    {
        auto result = std::vector<std::reference_wrapper<const TypeInfo>>();
        result.reserve(_types.size());
        for (const auto& type : _types)
            result.push_back(std::cref(*type));
        return result;
    }

    std::optional<std::reference_wrapper<const TypeInfo>> TypeRegistry::find(
        uint64 name_hash) const
    {
        for (const auto& type : _types)
            if (type->name_hash == name_hash)
                return std::cref(*type);
        return {};
    }

    std::optional<std::reference_wrapper<const TypeInfo>> TypeRegistry::find(
        std::string_view name) const
    {
        return find(hash(name));
    }

    TypeRegistry& get_type_registry()
    {
        static TypeRegistry g_registry = {};
        return g_registry;
    }
}
