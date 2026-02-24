#include "tbx/physics/raycast.h"
#include "tbx/messages/dispatcher.h"

namespace tbx
{
    bool Raycast::try_cast(RaycastResult& out_result) const
    {
        out_result = RaycastResult {};

        auto* dispatcher = get_global_dispatcher();
        if (dispatcher == nullptr)
            return false;

        auto request = RaycastRequest(*this);
        request.not_handled_behavior = MessageNotHandledBehavior::WARN;
        const Result dispatch_result = dispatcher->send(request);
        if (!dispatch_result.succeeded() || request.state != MessageState::HANDLED)
            return false;

        out_result = request.result;
        return request.result;
    }

    RaycastResult Raycast::cast() const
    {
        auto raycast_result = RaycastResult {};
        try_cast(raycast_result);
        return raycast_result;
    }

    RaycastRequest::RaycastRequest(const Raycast& raycast_query)
        : raycast(raycast_query)
    {
    }

    RaycastRequest::~RaycastRequest() noexcept = default;
}
