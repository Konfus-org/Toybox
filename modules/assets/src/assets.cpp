#include "tbx/assets/assets.h"
#include <string>
#include <cstdio>

namespace tbx
{
    void warn_missing_dispatcher(std::string_view action)
    {
        std::string message = std::string("No global dispatcher available to ").append(action);
        std::fprintf(stderr, "Toybox warning: %s\n", message.c_str());
    }

    std::shared_future<Result> make_missing_dispatcher_future(std::string_view action)
    {
        std::promise<Result> promise;
        Result result = {};
        result.flag_failure(std::string("No global dispatcher available to ").append(action));
        promise.set_value(result);
        return promise.get_future().share();
    }
}
