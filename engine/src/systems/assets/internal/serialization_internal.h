#pragma once
#include "tbx/systems/assets/serialization.h"
#include <mutex>
#include <vector>

namespace tbx::internal
{
    static std::vector<AssetTypeRegistration>& asset_type_registrations()
    {
        static auto registrations = std::vector<AssetTypeRegistration> {};
        return registrations;
    }

    static std::mutex& asset_type_registration_mutex()
    {
        static auto mutex = std::mutex();
        return mutex;
    }

    static std::vector<SerializableTypeRegistration>& serializable_type_registrations()
    {
        static auto registrations = std::vector<SerializableTypeRegistration> {};
        return registrations;
    }

    static std::mutex& serializable_type_registration_mutex()
    {
        static auto mutex = std::mutex();
        return mutex;
    }
}
