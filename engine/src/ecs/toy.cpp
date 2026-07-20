#include "tbx/ecs/toy.h"
#include "tbx/core/hash.h"
#include "tbx/ecs/sandbox.h"

namespace tbx
{
    //// TOY ////

    const std::string& Toy::get_name() const
    {
        return _sandbox->get().get_registry().get<ToyHandle>(_id).name;
    }

    Uuid Toy::get_uuid() const
    {
        return _sandbox->get().get_registry().get<ToyHandle>(_id).uuid;
    }

    bool Toy::is_alive() const
    {
        return _sandbox.has_value() && _sandbox->get().get_registry().valid(_id);
    }

    bool Toy::has_sticker(const std::string_view name) const
    {
        const auto* stickers = _sandbox->get().get_registry().try_get<StickerSet>(_id);
        if (!stickers)
            return false;
        const uint64 wanted = hash_name(name);
        for (const std::string& sticker : stickers->names)
            if (hash_name(sticker) == wanted)
                return true;
        return false;
    }

    void Toy::remove_sticker(const std::string_view name)
    {
        auto* stickers = _sandbox->get().get_registry().try_get<StickerSet>(_id);
        if (!stickers)
            return;
        const uint64 wanted = hash_name(name);
        std::erase_if(
            stickers->names,
            [wanted](const std::string& sticker) { return hash_name(sticker) == wanted; });
    }

    void Toy::set_name(std::string name)
    {
        _sandbox->get().get_registry().get<ToyHandle>(_id).name = std::move(name);
    }

    Toy& Toy::sticker(std::string name)
    {
        auto& stickers = _sandbox->get().get_registry().get_or_emplace<StickerSet>(_id);
        if (!has_sticker(name))
            stickers.names.push_back(std::move(name));
        return *this;
    }
}
