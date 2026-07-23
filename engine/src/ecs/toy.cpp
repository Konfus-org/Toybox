#include "tbx/ecs/toy.h"
#include "tbx/utils/hash.h"
#include "tbx/math/math.h"
#include "tbx/math/transform.h"
#include "tbx/reflection/type_registry.h"

namespace tbx
{
    ToyInfo& Toy::get_info() const
    {
        return _registry->get().get<ToyInfo>(_id);
    }

    const std::string& Toy::get_name() const
    {
        return get_info().name;
    }

    Uuid Toy::get_uuid() const
    {
        return get_info().uuid;
    }

    Transform& Toy::get_transform()
    {
        return _registry->get().get_or_emplace<Transform>(_id);
    }

    Mat4 Toy::get_world_transform() const
    {
        // A pure read walking the parent chain by id: every toy gets its Transform at add,
        // so a missing one is a default, never an insertion.
        Registry& registry = _registry->get();
        auto world = Mat4(1.0f);
        for (auto at = _id; at != NULL_TOY && registry.valid(at);)
        {
            const auto* transform = registry.try_get<Transform>(at);
            const auto local = transform ? *transform : Transform {};
            world = translate(Mat4(1.0f), local.position) * to_mat4(local.rotation)
                    * scale(Mat4(1.0f), local.scale) * world;
            const auto* info = registry.try_get<ToyInfo>(at);
            at = info ? info->parent : NULL_TOY;
        }
        return world;
    }

    std::optional<Toy> Toy::get_parent() const
    {
        Registry& registry = _registry->get();
        const auto* info = registry.try_get<ToyInfo>(_id);
        if (!info || info->parent == NULL_TOY || !registry.valid(info->parent))
            return {};
        return Toy(registry, info->parent);
    }

    std::vector<Toy> Toy::get_children() const
    {
        Registry& registry = _registry->get();
        auto children = std::vector<Toy>();
        for (const auto [id, info] : registry.view<ToyInfo>().each())
            if (info.parent == _id)
                children.emplace_back(registry, id);
        return children;
    }

    bool Toy::is_enabled() const
    {
        return get_info().is_enabled;
    }

    Toy& Toy::set_enabled(const bool is_enabled)
    {
        get_info().is_enabled = is_enabled;
        return *this;
    }

    bool Toy::is_alive() const
    {
        return _registry.has_value() && _registry->get().valid(_id);
    }

    bool Toy::has(const std::string_view name) const
    {
        const uint64 wanted = hash(name);
        for (const std::string& sticker : get_info().stickers)
            if (hash(sticker) == wanted)
                return true;
        return false;
    }

    Toy& Toy::remove(const std::string_view name)
    {
        const uint64 wanted = hash(name);
        std::erase_if(
            get_info().stickers,
            [wanted](const std::string& sticker) { return hash(sticker) == wanted; });
        return *this;
    }

    Toy& Toy::set_name(std::string name)
    {
        get_info().name = std::move(name);
        return *this;
    }

    Toy& Toy::set_parent(Toy parent)
    {
        // Parenting is one field on the toy's ToyInfo (a default/dead parent clears it).
        get_info().parent = parent.is_alive() ? parent.get_id() : NULL_TOY;
        return *this;
    }

    Toy& Toy::add(std::string name)
    {
        if (!has(name))
            get_info().stickers.push_back(std::move(name));
        return *this;
    }

    //// REFLECTED-BY-NAME BLOCK ACCESS ////

    std::byte* Toy::get_block_bytes(const uint64 type_hash) const
    {
        const auto type = describe_type(type_hash);
        if (!type || !type->get().get_block)
            return nullptr;
        return type->get().get_block(_registry->get(), _id);
    }

    std::byte* Toy::add_block_bytes(const uint64 type_hash)
    {
        const auto type = describe_type(type_hash);
        if (!type || !type->get().add_block)
            return nullptr;
        return type->get().add_block(_registry->get(), _id);
    }

    bool Toy::has_block_named(const uint64 type_hash) const
    {
        const auto type = describe_type(type_hash);
        return type && type->get().has_block && type->get().has_block(_registry->get(), _id);
    }

    void Toy::remove_block_named(const uint64 type_hash)
    {
        const auto type = describe_type(type_hash);
        if (type && type->get().remove_block)
            type->get().remove_block(_registry->get(), _id);
    }
}
