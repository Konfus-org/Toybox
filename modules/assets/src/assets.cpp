#include "tbx/assets/assets.h"
#include <string>

namespace tbx
{
    Result dispatcher_missing_result(std::string_view action)
    {
        Result result;
        result.flag_failure(std::string("No global dispatcher available to ").append(action));
        return result;
    }

    std::shared_future<Result> make_missing_dispatcher_future(std::string_view action)
    {
        std::promise<Result> promise;
        promise.set_value(dispatcher_missing_result(action));
        return promise.get_future().share();
    }

}
