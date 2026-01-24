#include "tbx/assets/assets.h"
#include <stdexcept>
#include <string>

namespace tbx
{
    void throw_missing_dispatcher(std::string_view action)
    {
        throw std::runtime_error(
            std::string("No global dispatcher available to ").append(action));
    }

    std::shared_future<Result> make_missing_dispatcher_future(std::string_view action)
    {
        std::promise<Result> promise;
        promise.set_exception(std::make_exception_ptr(
            std::runtime_error(std::string("No global dispatcher available to ").append(action))));
        return promise.get_future().share();
    }
}
