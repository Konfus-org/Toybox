#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/input/input_manager.h"
#include "tbx/systems/input/input_map.h"
#include <string>

namespace
{
    tbx::InputMap make_editor_map()
    {
        auto save_action = tbx::InputAction("editor.save", tbx::InputActionValueType::BUTTON);
        save_action.add_binding(tbx::InputBinding {
            .control = tbx::KeyChordInputControl { .key = tbx::InputKey::S, .ctrl = true },
        });

        auto move_action = tbx::InputAction("player.move", tbx::InputActionValueType::VECTOR2);
        move_action.add_binding(tbx::InputBinding {
            .control =
                tbx::KeyboardVector2CompositeInputControl {
                    .up = tbx::InputKey::W,
                    .down = tbx::InputKey::S,
                    .left = tbx::InputKey::A,
                    .right = tbx::InputKey::D,
                },
        });

        auto scheme = tbx::InputScheme("Editor", { save_action, move_action });
        scheme.set_is_active(true);

        auto map = tbx::InputMap();
        map.schemes.push_back(scheme);
        return map;
    }

    TEST(InputMapTests, RegistersAssetType)
    {
        const auto registration = tbx::get_asset_type_registration("InputMap");
        ASSERT_TRUE(registration.has_value());
        EXPECT_EQ(registration->type_name, "InputMap");
        EXPECT_EQ(registration->version, 1U);
    }

    TEST(InputMapTests, UnknownAssetTypeNameIsNotRegistered)
    {
        const auto registration = tbx::get_asset_type_registration("NotARegisteredAssetType");
        EXPECT_FALSE(registration.has_value());
    }

    TEST(InputMapTests, RoundTripsSchemesActionsAndBindings)
    {
        const auto map = make_editor_map();

        auto json = tbx::Json();
        tbx::serialize(json, map);

        // The asset body is a keyed document (never the bare single-field value), so editors can
        // read it per-field.
        ASSERT_TRUE(json.is_object());
        ASSERT_TRUE(json.contains("schemes"));

        auto restored = tbx::InputMap();
        tbx::deserialize(json, restored);

        ASSERT_EQ(restored.schemes.size(), 1U);
        const auto& scheme = restored.schemes.front();
        EXPECT_EQ(scheme.get_name(), "Editor");
        EXPECT_TRUE(scheme.get_is_active());

        const auto save_action = scheme.try_get_action("editor.save");
        ASSERT_TRUE(save_action.has_value());
        EXPECT_EQ(save_action->get().get_value_type(), tbx::InputActionValueType::BUTTON);
        ASSERT_EQ(save_action->get().get_bindings().size(), 1U);

        const auto& chord_control = save_action->get().get_bindings().front().control;
        ASSERT_TRUE(std::holds_alternative<tbx::KeyChordInputControl>(chord_control));
        const auto chord = std::get<tbx::KeyChordInputControl>(chord_control);
        EXPECT_EQ(chord.key, tbx::InputKey::S);
        EXPECT_TRUE(chord.ctrl);
        EXPECT_FALSE(chord.shift);

        const auto move_action = scheme.try_get_action("player.move");
        ASSERT_TRUE(move_action.has_value());
        EXPECT_EQ(move_action->get().get_value_type(), tbx::InputActionValueType::VECTOR2);
        ASSERT_EQ(move_action->get().get_bindings().size(), 1U);
        const auto& composite_control = move_action->get().get_bindings().front().control;
        ASSERT_TRUE(
            std::holds_alternative<tbx::KeyboardVector2CompositeInputControl>(composite_control));
        EXPECT_EQ(
            std::get<tbx::KeyboardVector2CompositeInputControl>(composite_control).right,
            tbx::InputKey::D);
    }

    TEST(InputMapTests, ReadsEnumsWrittenAsIntegers)
    {
        // Editor-authored files write enums numerically; the generated enum reader accepts both
        // the engine's string tokens and the underlying values. Rewrite the chord's key token to
        // its numeric value and confirm the map still loads it.
        const auto map = make_editor_map();
        auto json = tbx::Json();
        tbx::serialize(json, map);

        // The variant field wraps twice: { type: "variant", value: { type: <alternative>, value } }.
        auto& chord_value =
            json.at("schemes").at("value").at(0).at("actions").at("value").at(0).at("bindings")
                .at("value").at(0).at("control").at("value").at("value");
        ASSERT_EQ(chord_value.at("key").at("value"), "S");
        chord_value.at("key").at("value") = static_cast<int>(tbx::InputKey::S);
        json.at("schemes").at("value").at(0).at("actions").at("value").at(0).at("value_type")
            .at("value") = static_cast<int>(tbx::InputActionValueType::BUTTON);

        auto restored = tbx::InputMap();
        tbx::deserialize(json, restored);
        const auto action = restored.schemes.front().try_get_action("editor.save");
        ASSERT_TRUE(action.has_value());
        const auto& control = action->get().get_bindings().front().control;
        ASSERT_TRUE(std::holds_alternative<tbx::KeyChordInputControl>(control));
        EXPECT_EQ(std::get<tbx::KeyChordInputControl>(control).key, tbx::InputKey::S);
    }

    TEST(InputMapTests, ChordRequiresExactModifierState)
    {
        auto manager = tbx::InputManager();
        for (const tbx::InputScheme& scheme : make_editor_map().schemes)
            manager.add_scheme(scheme);

        const auto delta_time = tbx::DeltaTime { .seconds = 0.016, .milliseconds = 16.0 };
        auto input = tbx::ExternalInput();
        input.enabled = true;

        const auto is_save_active = [&]()
        {
            const auto scheme = manager.get_scheme("Editor");
            const auto action = scheme->get().try_get_action("editor.save");
            return action->get().get_is_active();
        };

        // Ctrl+S matches the chord.
        input.keyboard.pressed_keys = {
            static_cast<int>(tbx::InputKey::S),
            static_cast<int>(tbx::InputKey::LCTRL),
        };
        manager.set_external_input(input);
        manager.update(delta_time);
        EXPECT_TRUE(is_save_active());

        // An extra modifier (Ctrl+Shift+S) must not fire a Ctrl+S chord.
        input.keyboard.pressed_keys.insert(static_cast<int>(tbx::InputKey::LSHIFT));
        manager.set_external_input(input);
        manager.update(delta_time);
        EXPECT_FALSE(is_save_active());

        // A bare S must not fire it either.
        input.keyboard.pressed_keys = { static_cast<int>(tbx::InputKey::S) };
        manager.set_external_input(input);
        manager.update(delta_time);
        EXPECT_FALSE(is_save_active());
    }
}
