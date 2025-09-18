#pragma once
#include <memory>
#include <type_traits>

template <typename T>
struct is_smart_ptr : std::false_type {};

template <typename U>
struct is_smart_ptr<std::shared_ptr<U>> : std::true_type {};

template <typename U>
struct is_smart_ptr<std::weak_ptr<U>> : std::true_type {};
