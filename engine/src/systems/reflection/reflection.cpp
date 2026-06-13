#include "tbx/systems/reflection/reflection.h"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace tbx
{
    // Reflection records are stored behind unique_ptr so their addresses stay stable as the registry
    // grows: ReflectionInfo and PropertyInfo handles hold raw pointers into these records. Registration
    // is append-only at static-init; records are only erased on plugin teardown, by which point no
    // handle into a torn-down plugin's types should remain live. Lookups return raw pointers that
    // outlive the lock under that same append-only contract.
    class ReflectionRegistrationStore final
    {
      public:
        static ReflectionRegistrationStore& get_instance()
        {
            static ReflectionRegistrationStore store = {};
            return store;
        }

      public:
        ReflectionRegistrationStore(const ReflectionRegistrationStore&) = delete;
        ReflectionRegistrationStore& operator=(const ReflectionRegistrationStore&) = delete;
        ReflectionRegistrationStore(ReflectionRegistrationStore&&) = delete;
        ReflectionRegistrationStore& operator=(ReflectionRegistrationStore&&) = delete;

      public:
        std::mutex& mutex()
        {
            return _mutex;
        }

        std::vector<std::unique_ptr<TypeReflection>>& records()
        {
            return _records;
        }

        std::unordered_map<std::type_index, TypeReflection*>& by_type()
        {
            return _by_type;
        }

        std::unordered_map<std::string, TypeReflection*>& by_name()
        {
            return _by_name;
        }

      private:
        ReflectionRegistrationStore() = default;
        ~ReflectionRegistrationStore() noexcept = default;

      private:
        std::mutex _mutex = {};
        std::vector<std::unique_ptr<TypeReflection>> _records = {};
        std::unordered_map<std::type_index, TypeReflection*> _by_type = {};
        std::unordered_map<std::string, TypeReflection*> _by_name = {};
    };

    void register_type_reflection_entry(TypeReflection record)
    {
        if (record.name.empty())
            return;

        auto& store = ReflectionRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.mutex());
        if (store.by_name().contains(record.name))
            return;

        auto owned = std::make_unique<TypeReflection>(std::move(record));
        auto* pointer = owned.get();
        store.by_name().emplace(pointer->name, pointer);
        if (pointer->type != std::type_index(typeid(void)))
            store.by_type().emplace(pointer->type, pointer);
        store.records().push_back(std::move(owned));
    }

    void unregister_type_reflection_entry(std::string_view name)
    {
        if (name.empty())
            return;

        auto& store = ReflectionRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.mutex());
        const auto name_iterator = store.by_name().find(std::string(name));
        if (name_iterator == store.by_name().end())
            return;

        auto* record = name_iterator->second;
        store.by_name().erase(name_iterator);
        if (record != nullptr)
            store.by_type().erase(record->type);

        auto& records = store.records();
        for (auto iterator = records.begin(); iterator != records.end(); ++iterator)
        {
            if (iterator->get() == record)
            {
                records.erase(iterator);
                break;
            }
        }
    }

    void clear_type_reflections()
    {
        auto& store = ReflectionRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.mutex());
        store.by_name().clear();
        store.by_type().clear();
        store.records().clear();
    }

    std::vector<TypeReflection> get_type_reflections()
    {
        auto& store = ReflectionRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.mutex());
        auto result = std::vector<TypeReflection>();
        result.reserve(store.records().size());
        for (const auto& record : store.records())
            result.push_back(*record);
        return result;
    }

    const TypeReflection* find_type_reflection(std::type_index type)
    {
        auto& store = ReflectionRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.mutex());
        const auto iterator = store.by_type().find(type);
        return iterator == store.by_type().end() ? nullptr : iterator->second;
    }

    const TypeReflection* find_type_reflection(std::string_view wire_name)
    {
        if (wire_name.empty())
            return nullptr;

        auto& store = ReflectionRegistrationStore::get_instance();
        auto guard = std::lock_guard(store.mutex());
        const auto iterator = store.by_name().find(std::string(wire_name));
        return iterator == store.by_name().end() ? nullptr : iterator->second;
    }

    ReflectionInfo reflect(std::string_view wire_name)
    {
        return ReflectionInfo(find_type_reflection(wire_name));
    }

    ReflectionInfo reflect(std::type_index type, void* instance)
    {
        return ReflectionInfo(find_type_reflection(type), instance, false);
    }

    ReflectionInfo reflect(std::type_index type, const void* instance)
    {
        return ReflectionInfo(find_type_reflection(type), const_cast<void*>(instance), true);
    }
}
