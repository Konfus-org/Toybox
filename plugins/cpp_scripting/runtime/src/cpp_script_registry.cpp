#include "tbx/cpp_scripting/cpp_script_registry.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace tbx
{
    static std::unordered_map<std::string, CppScriptRegistration>& cpp_script_registrations()
    {
        static auto g_registrations = std::unordered_map<std::string, CppScriptRegistration> {};
        return g_registrations;
    }

    static std::mutex& cpp_script_registry_mutex()
    {
        static auto g_mutex = std::mutex {};
        return g_mutex;
    }

    void register_cpp_script_entry(std::string type_name, CppScriptRegistration entry)
    {
        auto guard = std::lock_guard(cpp_script_registry_mutex());
        cpp_script_registrations().insert_or_assign(std::move(type_name), entry);
    }

    const CppScriptRegistration* get_cpp_script_registration(std::string_view type_name)
    {
        auto guard = std::lock_guard(cpp_script_registry_mutex());
        const auto iterator = cpp_script_registrations().find(std::string(type_name));
        if (iterator == cpp_script_registrations().end())
            return nullptr;

        return &iterator->second;
    }

    void clear_cpp_script_registrations()
    {
        auto guard = std::lock_guard(cpp_script_registry_mutex());
        cpp_script_registrations().clear();
    }
}
