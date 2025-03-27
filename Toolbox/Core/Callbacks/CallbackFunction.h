#pragma once

template <typename TArg>
using CallbackFunction = std::function<void(TArg&)>;
