#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/assets/asset_handle.h"
#include "tbx/types/assets/material.h"

// Positive: a valid asset handle persists its id through the handle serializer and reads back valid.
TEST(AssetHandleTests, ValidAssetHandleRoundTripsId)
{
    const auto original = tbx::AssetHandle<tbx::Material>(tbx::Handle("Materials/Stone.mat"));

    const auto json = tbx::write_serialization_value<tbx::Json>(original);
    auto restored = tbx::AssetHandle<tbx::Material>();
    tbx::read_serialization_value(json, restored);

    EXPECT_TRUE(original.is_valid());
    EXPECT_TRUE(restored.is_valid());
    EXPECT_EQ(restored.get_id(), original.get_id());
}

// Negative: an unset asset handle is invalid and, deserialized over a previously-set one, clears it back
// to the empty, still-invalid handle.
TEST(AssetHandleTests, UnsetAssetHandleRoundTripsInvalid)
{
    const auto original = tbx::AssetHandle<tbx::Material>();

    const auto json = tbx::write_serialization_value<tbx::Json>(original);
    auto restored = tbx::AssetHandle<tbx::Material>(tbx::Handle("Materials/Stone.mat"));
    tbx::read_serialization_value(json, restored);

    EXPECT_FALSE(original.is_valid());
    EXPECT_FALSE(restored.is_valid());
    EXPECT_EQ(restored.get_id(), tbx::Uuid());
}

// Positive: a typed asset handle advertises its own editor token, so the describe layer tells the
// inspector which picker to show (distinct from a plain entity reference's "entity").
TEST(AssetHandleTests, TypedHandlesCarryTheirEditorToken)
{
    EXPECT_EQ(tbx::get_property_type_token<tbx::AssetHandle<tbx::Material>>(), "asset");
}

// Negative: a plain (non-handle) value keeps its own token — the asset token is handle-only.
TEST(AssetHandleTests, NonHandleValueDoesNotCarryAnAssetToken)
{
    EXPECT_NE(tbx::get_property_type_token<int>(), "asset");
}

// Positive: the handle reads BOTH the bare-id form (as the engine persists it) and the { "id": ... }
// object form (as the editor sync sends a handle) — so migrating a Handle field to AssetHandle never drops
// a value coming from either path.
TEST(AssetHandleTests, ReadsBothTheBareIdAndTheEditorObjectForm)
{
    const auto expected = tbx::Handle("Materials/Stone.mat").id;

    const auto bare_form = tbx::write_serialization_value<tbx::Json>(expected);
    auto from_bare = tbx::AssetHandle<tbx::Material>();
    tbx::read_serialization_value(bare_form, from_bare);

    auto object_form = tbx::Json::object();
    object_form["id"] = tbx::write_serialization_value<tbx::Json>(expected);
    auto from_object = tbx::AssetHandle<tbx::Material>();
    tbx::read_serialization_value(object_form, from_object);

    EXPECT_EQ(from_bare.get_id(), expected);
    EXPECT_EQ(from_object.get_id(), expected);
}

// Negative: reading an object that carries no id leaves the handle invalid rather than throwing.
TEST(AssetHandleTests, ReadingAnIdlessObjectLeavesTheHandleInvalid)
{
    auto idless_object = tbx::Json::object();
    auto handle = tbx::AssetHandle<tbx::Material>(tbx::Handle("Materials/Stone.mat"));
    tbx::read_serialization_value(idless_object, handle);

    EXPECT_FALSE(handle.is_valid());
}
