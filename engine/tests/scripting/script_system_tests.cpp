#include "in_memory_file_ops.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/files/json.h"
#include "tbx/systems/scripting/script_system.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/script_container.h"
#include <filesystem>
#include <memory>

namespace tbx::tests::scripting
{
    class NullMessageDispatcher final : public IMessageDispatcher
    {
      protected:
        Result send(Message&) const override
        {
            return {};
        }

        std::shared_future<Result> post(std::unique_ptr<Message>) const override
        {
            auto promise = std::promise<Result> {};
            promise.set_value(Result());
            return promise.get_future().share();
        }
    };

    class DoorScript final : public Script
    {
      public:
        float open_speed = 1.0F;
        int start_count = 0;
        int update_count = 0;

      public:
        void on_start() override
        {
            ++start_count;
        }

        void on_update(const DeltaTime&) override
        {
            ++update_count;
        }

        void open()
        {
            open_speed += 1.0F;
        }
    };

    inline constexpr std::string_view tbx_serialization_type_name(const DoorScript*)
    {
        return "door_controller";
    }

    inline void serialize(Json& json, const DoorScript& value)
    {
        json["open_speed"] = value.open_speed;
    }

    inline void deserialize(const Json& json, DoorScript& value)
    {
        const auto default_value = DoorScript();
        read_serialization_field(json, "open_speed", value.open_speed, default_value.open_speed);
    }

    static Result apply_door_overrides(const Json& json, DoorScript& value)
    {
        if (const auto open_speed = json.find("open_speed"); open_speed != json.end())
            value.open_speed = open_speed->get<float>();
        return {};
    }

    static void bind_door_runtime(DoorScript&, ScriptContext&) {}

    static void ensure_door_script_registered()
    {
        static const bool registered =
            register_script_asset_type<DoorScript>(1U, apply_door_overrides, bind_door_runtime);
        static_cast<void>(registered);
    }

    class FakeResolver final : public IScriptResolver
    {
      public:
        std::weak_ptr<Script> try_get_script(const ScriptLookup& lookup) override
        {
            last_lookup = lookup;
            return script;
        }

        std::shared_ptr<DoorScript> script = {};
        ScriptLookup last_lookup = {};
    };

    TEST(ScriptingTests, ScriptContainerSerializesUuidOnlyBindings)
    {
        // Arrange
        ensure_door_script_registered();
        auto container = ScriptContainer();
        auto binding = ScriptContainerBinding();
        binding.script = Uuid(0x41000001U);
        binding.binding_id = Uuid(7U);
        binding.overrides["open_speed"] = 12.0F;
        container.scripts.push_back(binding);

        // Act
        auto json = Json();
        serialize(json, container);
        auto roundtripped = ScriptContainer();
        deserialize(json, roundtripped);

        // Assert
        EXPECT_EQ(json["scripts"][0]["script"].get<uint32>(), 0x41000001U);
        ASSERT_EQ(roundtripped.scripts.size(), 1U);
        EXPECT_EQ(roundtripped.scripts[0].script, Uuid(0x41000001U));
        EXPECT_FLOAT_EQ(roundtripped.scripts[0].overrides["open_speed"].get<float>(), 12.0F);
    }

    TEST(ScriptingTests, WeakScriptReferenceBindingResolvesPointer)
    {
        // Arrange
        ensure_door_script_registered();
        auto resolver = FakeResolver();
        resolver.script = std::make_shared<DoorScript>();
        auto services = ServiceProvider();
        auto owner = Entity();
        auto context = ScriptContext(Uuid(1U), owner, {}, services, resolver);
        auto owner_script = DoorScript();
        owner_script.set_script_reference(
            "linked_door",
            ScriptBinding {
                .entity = Uuid(70U),
                .script = Uuid(0x41000001U),
                .binding_id = Uuid(9U),
            });
        auto reference = std::weak_ptr<DoorScript>();
        bind_script_reference_field(owner_script, "linked_door", reference, context);

        // Act
        auto resolved = reference.lock();

        // Assert
        ASSERT_NE(resolved, nullptr);
        resolved->open();
        EXPECT_FLOAT_EQ(resolver.script->open_speed, 2.0F);
        EXPECT_EQ(resolver.last_lookup.entity, Uuid(70U));
        EXPECT_EQ(resolver.last_lookup.script, Uuid(0x41000001U));
        EXPECT_EQ(resolver.last_lookup.binding_id, Uuid(9U));
    }

    TEST(ScriptingTests, ScriptSystemCreatesPerEntityInstanceFromScriptAsset)
    {
        // Arrange
        ensure_door_script_registered();
        auto file_ops = std::make_shared<InMemoryFileOps>("/virtual/worlds");
        file_ops->set_text("main.world.meta", R"({ "id": { "value": 32 }, "version": 1 })");
        file_ops->set_text(
            "main.world",
            R"({
                "globals": { "name": "main.globals", "id": { "value": 33 } },
                "chunks": []
            })");
        file_ops->set_text("main.globals.meta", R"({ "id": { "value": 33 }, "version": 1 })");
        file_ops->set_text(
            "main.globals",
            R"({
                "entities": [
                    {
                        "id": { "value": 40 },
                        "name": "Door",
                        "tag": "",
                        "layer": "",
                        "parent": { "value": 0 },
                        "components": {
                            "script_container": {
                                "id": { "value": 41 },
                                "scripts": [
                                    {
                                        "script": { "value": 1090519041 },
                                        "enabled": true,
                                        "binding_id": { "value": 1 },
                                        "overrides": { "open_speed": 12.0 }
                                    }
                                ]
                            }
                        }
                    }
                ]
            })");
        file_ops->set_text(
            "Scripts/Door.script.meta",
            R"({ "id": { "value": 1090519041 }, "version": 1, "type": "door_controller" })");
        file_ops->set_text("Scripts/Door.script", R"({ "open_speed": 2.0 })");

        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>(file_ops);
        auto asset_manager = std::make_shared<AssetManager>(
            dispatcher,
            serialization_registry,
            "/virtual/worlds",
            std::vector<std::filesystem::path> {"."},
            HandleSource(),
            file_ops);
        auto services = ServiceProvider();
        auto script_system = ScriptSystem(asset_manager, services);
        ASSERT_EQ(
            asset_manager->ensure(Handle("Scripts/Door.script", Uuid(1090519041U))),
            Uuid(1090519041U));
        const auto world = asset_manager->load<World>(Handle("main.world", Uuid(32U)));
        const auto globals = asset_manager->load<WorldGlobals>(Handle("main.globals", Uuid(33U)));
        ASSERT_NE(world, nullptr);
        ASSERT_NE(globals, nullptr);
        world->load_globals(*globals);

        // Act
        script_system.update(DeltaTime {.seconds = 0.016, .milliseconds = 16.0});
        auto script = std::dynamic_pointer_cast<DoorScript>(script_system
                                                                .try_get_script(
                                                                    ScriptLookup {
                                                                        .world = Uuid(32U),
                                                                        .entity = Uuid(40U),
                                                                        .script = Uuid(1090519041U),
                                                                        .binding_id = Uuid(1U),
                                                                    })
                                                                .lock());

        // Assert
        ASSERT_NE(script, nullptr);
        EXPECT_EQ(script->start_count, 1);
        EXPECT_EQ(script->update_count, 1);
        EXPECT_FLOAT_EQ(script->open_speed, 12.0F);
    }
}
