#include "tbx/reflection/reflection.h"
#include "tbx/debug/log.h"

namespace tbx::reflection
{
    //// TYPE REGISTRY ////

    TypeInfo& TypeRegistry::add(TypeInfo info)
    {
        // Idempotent on purpose: a name registered twice keeps its one record, so facets
        // stack — register_asset<App>("App") stamps the asset facet onto the same TypeInfo
        // register_type<App> built the .tapp schema on.
        for (auto& existing : _types)
            if (existing->name_hash == info.name_hash)
                return *existing;
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

    std::optional<std::reference_wrapper<const TypeInfo>> describe(const uint64 name_hash)
    {
        return get_type_registry().find(name_hash);
    }

    std::optional<std::reference_wrapper<const TypeInfo>> describe(const std::string_view name)
    {
        return get_type_registry().find(name);
    }
}
