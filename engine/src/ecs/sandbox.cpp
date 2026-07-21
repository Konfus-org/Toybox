#include "tbx/ecs/sandbox.h"
#include "tbx/app.h"
#include "tbx/core/log.h"

namespace tbx
{
    //// SANDBOX: TOYS ////

    Sandbox::Sandbox(Jobs& jobs)
        : _jobs(jobs)
    {
        register_builtin_blocks();
    }

    Toy Sandbox::spawn(std::string name)
    {
        const ToyId id = _registry.create();
        _registry.emplace<ToyHandle>(
            id,
            ToyHandle {.uuid = Uuid::generate(), .name = std::move(name)});
        _registry.emplace<Transform>(id);
        return Toy(*this, id);
    }

    void Sandbox::despawn(Toy toy)
    {
        const ToyId id = toy.get_id();
        // Children are orphaned, not destroyed — despawning a parent never cascades. Collect
        // first: removing the iterated component mid-view is not safe.
        auto orphans = std::vector<ToyId>();
        for (const auto [child, link] : _registry.view<ParentLink>().each())
            if (link.parent == id)
                orphans.push_back(child);
        for (const ToyId child : orphans)
            _registry.remove<ParentLink>(child);
        _registry.destroy(id);
    }

    std::optional<Toy> Sandbox::find(const Uuid& uuid)
    {
        for (const auto [id, identity] : _registry.view<ToyHandle>().each())
            if (identity.uuid == uuid)
                return Toy(*this, id);
        return {};
    }

    std::optional<Toy> Sandbox::find(std::string_view name)
    {
        for (const auto [id, identity] : _registry.view<ToyHandle>().each())
            if (identity.name == name)
                return Toy(*this, id);
        return {};
    }

    void Sandbox::for_each_sticker(
        std::string_view name,
        const std::function<void(Toy)>& callback)
    {
        const uint64 wanted = hash(name);
        for (const auto [id, stickers] : _registry.view<StickerSet>().each())
            for (const std::string& sticker : stickers.names)
                if (hash(sticker) == wanted)
                {
                    callback(Toy(*this, id));
                    break;
                }
    }

    size Sandbox::get_toy_count() const
    {
        return _registry.view<const ToyHandle>().size();
    }

    //// SANDBOX: HIERARCHY ////

    void Sandbox::set_parent(Toy child, Toy parent)
    {
        if (parent.is_alive())
            _registry.emplace_or_replace<ParentLink>(
                child.get_id(),
                ParentLink {.parent = parent.get_id()});
        else
            _registry.remove<ParentLink>(child.get_id());
    }

    std::optional<Toy> Sandbox::get_parent(Toy child)
    {
        const auto* link = _registry.try_get<ParentLink>(child.get_id());
        if (!link || !_registry.valid(link->parent))
            return {};
        return Toy(*this, link->parent);
    }

    Mat4 Sandbox::get_world_matrix(Toy toy)
    {
        const auto& transform = _registry.get_or_emplace<Transform>(toy.get_id());
        const Mat4 local = math::translate(Mat4(1.0f), transform.position)
            * math::to_mat4(transform.rotation) * math::scale(Mat4(1.0f), transform.scale);
        const auto parent = get_parent(toy);
        if (!parent)
            return local;
        return get_world_matrix(*parent) * local;
    }

    //// SANDBOX: KITS ////

    Json Sandbox::save_kit(std::span<const Toy> toys)
    {
        auto kit = Json::object();
        auto toys_json = Json::array();
        auto bounds_min = Vec3(0.0f);
        auto bounds_max = Vec3(0.0f);
        bool has_bounds = false;

        for (const Toy& toy : toys)
        {
            const ToyId id = toy.get_id();
            auto toy_json = Json::object();
            const auto& identity = _registry.get<ToyHandle>(id);
            toy_json["uuid"] = identity.uuid.to_string();
            toy_json["name"] = identity.name;
            if (!identity.is_enabled)
                toy_json["is_enabled"] = false;

            if (const auto* link = _registry.try_get<ParentLink>(id);
                link && _registry.valid(link->parent))
                toy_json["parent"] = _registry.get<ToyHandle>(link->parent).uuid.to_string();

            if (const auto* stickers = _registry.try_get<StickerSet>(id);
                stickers && !stickers->names.empty())
                toy_json["stickers"] = stickers->names;

            auto blocks = Json::array();
            for (const uint64 hash : get_block_registry().get_all_hashes())
            {
                const auto operations = get_block_registry().find(hash);
                const auto type = get_type_registry().find(hash);
                if (!operations || !type)
                    continue;
                if (!operations->has(_registry, id))
                    continue;
                blocks.push_back(json_write(type->get(), operations->get(_registry, id)));
            }
            toy_json["blocks"] = std::move(blocks);
            toys_json.push_back(std::move(toy_json));

            if (const auto* transform = _registry.try_get<Transform>(id))
            {
                bounds_min = has_bounds ? math::min(bounds_min, transform->position)
                                        : transform->position;
                bounds_max = has_bounds ? math::max(bounds_max, transform->position)
                                        : transform->position;
                has_bounds = true;
            }
        }

        kit["toys"] = std::move(toys_json);
        const Vec3 center = has_bounds ? (bounds_min + bounds_max) * 0.5f : Vec3(0.0f);
        const float radius = has_bounds ? math::length(bounds_max - center) : 0.0f;
        kit["bounds"] =
            Json {{"center", {center.x, center.y, center.z}}, {"radius", radius}};
        return kit;
    }

    Result<KitInstance> Sandbox::load_kit(
        const Json& kit,
        const Vec3& root_position,
        const KitResolver& resolver)
    {
        auto reference_stack = std::vector<uint64>();
        auto spawned = std::vector<ToyId>();
        auto result = load_kit_body(kit, root_position, resolver, reference_stack, spawned);
        if (!result)
        {
            for (const ToyId id : spawned)
                if (_registry.valid(id))
                    _registry.destroy(id);
            return result;
        }
        return result;
    }

    Result<KitInstance> Sandbox::load_kit_body(
        const Json& kit,
        const Vec3& root_position,
        const KitResolver& resolver,
        std::vector<uint64>& reference_stack,
        std::vector<ToyId>& spawned)
    {
        if (!kit.is_object())
            return fail("kit body is not a JSON object");

        const size first_spawned = spawned.size();
        auto by_kit_uuid = std::unordered_map<std::string, ToyId>();

        try
        {
            // Pass 1: spawn every toy with identity, stickers, and blocks.
            for (const Json& toy_json : kit.value("toys", Json::array()))
            {
                Toy toy = spawn(toy_json.value("name", std::string("Toy")));
                toy.set_enabled(toy_json.value("is_enabled", true));
                spawned.push_back(toy.get_id());
                by_kit_uuid[toy_json.value("uuid", std::string())] = toy.get_id();

                for (const Json& sticker : toy_json.value("stickers", Json::array()))
                    toy.sticker(sticker.get<std::string>());

                for (const Json& block_json : toy_json.value("blocks", Json::array()))
                {
                    const auto type_name = block_json.value("__type", std::string());
                    const uint64 hashed = hash(type_name);
                    const auto operations = get_block_registry().find(hashed);
                    const auto type = get_type_registry().find(hashed);
                    if (!operations || !type)
                    {
                        log_warn("kit references unknown block type '{}'; skipped", type_name);
                        continue;
                    }
                    std::byte* block = operations->add_default(_registry, toy.get_id());
                    auto read = json_read(type->get(), block, block_json);
                    if (!read)
                        return std::unexpected(read.error());
                }
            }

            // Pass 2: link parents by the kit file's uuids (fresh uuids were assigned live).
            for (const Json& toy_json : kit.value("toys", Json::array()))
            {
                if (!toy_json.contains("parent"))
                    continue;
                const auto child = by_kit_uuid.find(toy_json.value("uuid", std::string()));
                const auto parent = by_kit_uuid.find(toy_json["parent"].get<std::string>());
                if (child != by_kit_uuid.end() && parent != by_kit_uuid.end())
                    _registry.emplace_or_replace<ParentLink>(
                        child->second,
                        ParentLink {.parent = parent->second});
            }

            // Root offset applies to this body's parentless toys only.
            for (size i = first_spawned; i < spawned.size(); ++i)
            {
                const ToyId id = spawned[i];
                if (!_registry.all_of<ParentLink>(id))
                    _registry.get<Transform>(id).position += root_position;
            }

            // Recurse into nested kit references (a kit can reference a kit...).
            for (const Json& entry : kit.value("kits", Json::array()))
            {
                const auto reference = entry.value("reference", std::string());
                const uint64 reference_hash = hash(reference);
                for (const uint64 seen : reference_stack)
                    if (seen == reference_hash)
                        return fail("kit reference cycle detected at '{}'", reference);
                if (!resolver)
                    return fail(
                        "kit references '{}' but no resolver was provided",
                        reference);

                auto body = resolver(reference);
                if (!body)
                    return std::unexpected(body.error());

                auto position = root_position;
                if (entry.contains("position"))
                    position += Vec3(
                        entry["position"].at(0).get<float>(),
                        entry["position"].at(1).get<float>(),
                        entry["position"].at(2).get<float>());

                reference_stack.push_back(reference_hash);
                auto nested =
                    load_kit_body(*body, position, resolver, reference_stack, spawned);
                reference_stack.pop_back();
                if (!nested)
                    return nested;
            }
        }
        catch (const Json::exception& e)
        {
            return fail("malformed kit body: {}", e.what());
        }

        // Only the outermost body mints the instance; nested bodies fold into it.
        if (!reference_stack.empty())
            return KitInstance {.id = 0};
        const auto instance = KitInstance {.id = _next_kit_instance_id++};
        _kit_instances[instance.id] = spawned;
        return instance;
    }

    Result<KitInstance> Sandbox::spawn(
        const Json& kit,
        const Vec3& position,
        const KitResolver& resolver)
    {
        return load_kit(kit, position, resolver);
    }

    void Sandbox::despawn(KitInstance instance)
    {
        const auto it = _kit_instances.find(instance.id);
        if (it == _kit_instances.end())
            return;
        for (const ToyId id : it->second)
            if (_registry.valid(id))
                despawn(Toy(*this, id));
        _kit_instances.erase(it);
    }

    //// SANDBOX: LAYOUT & STREAMING ////

    void Sandbox::close()
    {
        _kit_instances.clear();
        _streamed_entries.clear();
        _layout_resolver = {};
        _has_stream_focus = false;
        _registry.clear(); // every toy, kit-spawned or not
    }

    Result<void> Sandbox::open(Layout layout)
    {
        const Json& body = layout.kits;
        const KitResolver& resolver = layout.resolver;
        if (!body.is_object())
            return fail("sandbox layout is not a JSON object");
        _layout_resolver = resolver;

        try
        {
            for (const Json& entry : body.value("kits", Json::array()))
            {
                const auto reference = entry.value("reference", std::string());
                auto position = Vec3(0.0f);
                if (entry.contains("position"))
                    position = Vec3(
                        entry["position"].at(0).get<float>(),
                        entry["position"].at(1).get<float>(),
                        entry["position"].at(2).get<float>());

                const bool is_streamed = entry.value("mode", std::string("always")) == "streamed";
                auto body = resolver(reference);
                if (!body)
                    return std::unexpected(body.error());

                if (!is_streamed)
                {
                    auto loaded = load_kit(*body, position, resolver);
                    if (!loaded)
                        return std::unexpected(loaded.error());
                    continue;
                }

                // Streamed: remember the entry + its saved bounds; the body reloads on demand.
                auto streamed = StreamedEntry {};
                streamed.reference = reference;
                streamed.position = position;
                const Json bounds = body->value("bounds", Json::object());
                if (bounds.contains("center"))
                    streamed.bounds_center = Vec3(
                        bounds["center"].at(0).get<float>(),
                        bounds["center"].at(1).get<float>(),
                        bounds["center"].at(2).get<float>());
                streamed.bounds_radius = bounds.value("radius", 0.0f);
                _streamed_entries.push_back(std::move(streamed));
            }
        }
        catch (const Json::exception& e)
        {
            return fail("malformed sandbox layout: {}", e.what());
        }
        return {};
    }

    void Sandbox::stream(const Vec3& focus)
    {
        _stream_focus = focus;
        _has_stream_focus = true;
        process_streaming();
    }

    void Sandbox::process_streaming()
    {
        if (!_has_stream_focus)
            return;

        for (size i = 0; i < _streamed_entries.size(); ++i)
        {
            StreamedEntry& entry = _streamed_entries[i];
            const float distance =
                math::length(_stream_focus - (entry.bounds_center + entry.position));
            const bool is_loaded = entry.instance.has_value();

            if (!is_loaded && !entry.is_loading
                && distance <= entry.bounds_radius + STREAM_LOAD_MARGIN)
            {
                entry.is_loading = true;
                // Resolve (file IO/parse) on a worker; splice on the main thread. The Sandbox
                // is engine-owned and outlives in-flight streams. The reference is copied into
                // the task — the worker never touches sandbox state.
                _jobs.get().start(
                    [](Sandbox& sandbox,
                       size index,
                       std::string reference,
                       KitResolver resolver) -> Task<void>
                    {
                        co_await sandbox._jobs.get().on_worker();
                        auto body = resolver(reference);
                        co_await sandbox._jobs.get().on_main();
                        StreamedEntry& target = sandbox._streamed_entries[index];
                        target.is_loading = false;
                        if (!body)
                        {
                            log_error("streamed kit '{}': {}", target.reference, body.error());
                            co_return;
                        }
                        auto loaded = sandbox.load_kit(*body, target.position, resolver);
                        if (!loaded)
                        {
                            log_error("streamed kit '{}': {}", target.reference, loaded.error());
                            co_return;
                        }
                        target.instance = *loaded;
                    }(*this, i, entry.reference, _layout_resolver));
            }
            else if (is_loaded && distance >= entry.bounds_radius + STREAM_UNLOAD_MARGIN)
            {
                despawn(*entry.instance);
                entry.instance.reset();
            }
        }
    }
}
