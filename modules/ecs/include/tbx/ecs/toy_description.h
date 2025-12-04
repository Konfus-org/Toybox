#pragma once
#include "tbx/common/uuid.h"
#include <any>
#include <string>
#include <vector>

namespace tbx
{
    using Sticker = std::string;
    using Block = std::any;

    // Represents a snapshot of a toy's identifying state.
    // Ownership: Value type; callers own their copies and are responsible for persisting any
    // required lifetime.
    // Thread-safety: Not synchronized. Expected to be used from the engine thread that owns the
    // ECS dispatcher.
    struct ToyDescription
    {
        ToyDescription() = default;
        ToyDescription(
            const std::string& display_name,
            const std::vector<Sticker>& display_stickers,
            const Uuid& parent_id,
            const Uuid& toy_id)
            : name(display_name)
            , stickers(display_stickers)
            , parent(parent_id)
            , id(toy_id)
        {
        }

        std::string name = "";
        std::vector<Sticker> stickers = {};
        Uuid parent = invalid::uuid;
        Uuid id = Uuid::generate();
    };

    namespace invalid
    {
        inline Block block = std::any();
        inline ToyDescription toy_description =
            ToyDescription("INVALID", {}, invalid::uuid, invalid::uuid);
    }
}
