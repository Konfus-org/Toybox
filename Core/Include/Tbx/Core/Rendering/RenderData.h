#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Rendering/RenderCommands.h"
#include <any>

namespace Tbx
{
    struct RenderData
    {
    public:
        EXPORT RenderData() = default;
        EXPORT RenderData(const RenderCommand& command, const std::any& payload)
            : _command(command), _payload(payload) {}

        EXPORT const RenderCommand& GetCommand() const { return _command; }
        EXPORT const std::any& GetPayload() const { return _payload; }

    private:
        RenderCommand _command = RenderCommand::None;
        std::any _payload = nullptr;
    };
}
