#pragma once
#include <functional>

template <typename TArg>
using CallbackFunction = std::function<void(TArg&)>;
