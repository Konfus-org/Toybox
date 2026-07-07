#pragma once
#include <string_view>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The editor wire vocabulary in one place — every JSON-RPC method name the bridge
    /// registers or notifies, plus the recurring JSON param/reply keys — mirroring the editor-side
    /// EngineMethods container. Call sites reference these constants instead of re-spelling the
    /// string, so the protocol is enumerable and a typo is a compile error rather than a silent
    /// wire mismatch.
    namespace Wire
    {
        // --- RPC method names, grouped by engine namespace (mirrors the editor's EngineMethods) ---

        inline constexpr std::string_view EDITOR_HELLO = "editor.hello";
        inline constexpr std::string_view EDITOR_LIST_ASSETS = "editor.listAssets";
        inline constexpr std::string_view EDITOR_MODEL_SLOTS = "editor.modelSlots";
        inline constexpr std::string_view EDITOR_PREVIEW_TEXTURE_MATERIAL =
            "editor.previewTextureMaterial";
        inline constexpr std::string_view EDITOR_LOG = "editor.log";

        inline constexpr std::string_view ENGINE_PING = "engine.ping";
        inline constexpr std::string_view ENGINE_SHUTDOWN = "engine.shutdown";
        inline constexpr std::string_view ENGINE_SET_PAUSED = "engine.setPaused";
        inline constexpr std::string_view ENGINE_SET_PLAYING = "engine.setPlaying";
        inline constexpr std::string_view ENGINE_SET_LOG_COLORS = "engine.setLogColors";
        inline constexpr std::string_view ENGINE_LOG = "engine.log";

        inline constexpr std::string_view WORLD_DESCRIBE = "world.describe";
        inline constexpr std::string_view WORLD_SAVE = "world.save";
        inline constexpr std::string_view WORLD_OPEN = "world.open";

        inline constexpr std::string_view ENTITY_CREATE = "entity.create";
        inline constexpr std::string_view ENTITY_DESCRIBE = "entity.describe";
        inline constexpr std::string_view ENTITY_DESTROY = "entity.destroy";
        inline constexpr std::string_view ENTITY_MOVE = "entity.move";
        inline constexpr std::string_view ENTITY_SET_COMPONENT = "entity.setComponent";
        inline constexpr std::string_view ENTITY_ADD_COMPONENT = "entity.addComponent";
        inline constexpr std::string_view ENTITY_REMOVE_COMPONENT = "entity.removeComponent";
        inline constexpr std::string_view ENTITY_ADD_SCRIPT = "entity.addScript";

        inline constexpr std::string_view ASSET_DESCRIBE = "asset.describe";
        inline constexpr std::string_view ASSET_SAVE = "asset.save";
        inline constexpr std::string_view ASSET_CREATE = "asset.create";
        inline constexpr std::string_view ASSET_FORGET = "asset.forget";
        inline constexpr std::string_view ASSET_NEW_ID = "asset.newId";
        inline constexpr std::string_view ASSET_PREVIEW_STATS = "asset.previewStats";
        inline constexpr std::string_view ASSET_PAIRING = "asset.pairing";
        inline constexpr std::string_view ASSET_GENERATE_MISSING_METAS =
            "asset.generateMissingMetas";

        inline constexpr std::string_view SYNC_CATALOG = "sync.catalog";
        inline constexpr std::string_view SYNC_DESCRIBE = "sync.describe";
        inline constexpr std::string_view SYNC_SET = "sync.set";
        inline constexpr std::string_view SYNC_RESET = "sync.reset";
        inline constexpr std::string_view SYNC_IS_DEFAULT = "sync.isDefault";

        inline constexpr std::string_view VIEW_START = "view.start";
        inline constexpr std::string_view VIEW_STOP = "view.stop";
        inline constexpr std::string_view VIEW_INPUT = "view.input";
        inline constexpr std::string_view VIEW_PICK = "view.pick";
        inline constexpr std::string_view VIEW_PICK_RECT = "view.pickRect";
        inline constexpr std::string_view VIEW_SET_SELECTION = "view.setSelection";
        inline constexpr std::string_view VIEW_PROJECT_ENTITIES = "view.projectEntities";
        inline constexpr std::string_view VIEW_QUERY_OCCLUSION = "view.queryOcclusion";
        inline constexpr std::string_view VIEW_SET_GIZMO = "view.setGizmo";
        inline constexpr std::string_view VIEW_FRAME_ASSET_PREVIEW = "view.frameAssetPreview";
        // Engine-to-editor notifications (sent through the RPC host, never registered).
        inline constexpr std::string_view VIEW_SURFACE = "view.surface";
        inline constexpr std::string_view VIEW_PRESENTED = "view.presented";
        inline constexpr std::string_view VIEW_TRANSFORM_EDITED = "view.transformEdited";
        inline constexpr std::string_view INPUT_MOUSE_LOCK = "input.mouseLock";

        inline constexpr std::string_view APP_DESCRIBE_SETTINGS = "app.describeSettings";

        // --- Recurring JSON param/reply keys ---

        // Addressing: which world/entity/asset/component/property (or sync path) an op targets.
        inline constexpr std::string_view WORLD_ASSET_ID = "worldAssetId";
        inline constexpr std::string_view ENTITY_ID = "entityId";
        inline constexpr std::string_view ASSET_ID = "assetId";
        inline constexpr std::string_view COMPONENT = "component";
        inline constexpr std::string_view PROPERTY = "property";
        inline constexpr std::string_view PATH = "path";
        inline constexpr std::string_view VIEW = "view";

        // Common param/reply fields.
        inline constexpr std::string_view ID = "id";
        inline constexpr std::string_view IDS = "ids";
        inline constexpr std::string_view NAME = "name";
        inline constexpr std::string_view TYPE = "type";
        inline constexpr std::string_view VALUE = "value";
        inline constexpr std::string_view BODY = "body";
        inline constexpr std::string_view VERSION = "version";
        inline constexpr std::string_view PARENT = "parent";
        inline constexpr std::string_view INDEX = "index";
        inline constexpr std::string_view SCRIPT = "script";
        inline constexpr std::string_view SCRIPTS = "scripts";
        inline constexpr std::string_view TAGS = "tags";
        inline constexpr std::string_view GLOBAL = "global";
        inline constexpr std::string_view ENABLED = "enabled";
        inline constexpr std::string_view MODE = "mode";
        inline constexpr std::string_view FORMAT = "format";
        inline constexpr std::string_view SETTINGS = "settings";
        inline constexpr std::string_view SLOTS = "slots";
        inline constexpr std::string_view LEVEL = "level";
        inline constexpr std::string_view MESSAGE = "message";
        inline constexpr std::string_view OCCLUDED = "occluded";
        // The reply key of sync.isDefault (distinct from the describe shape's "is_default" below).
        inline constexpr std::string_view IS_DEFAULT_REPLY = "isDefault";

        // The describe payload shape: entity bodies, per-field wrappers and the icon side table.
        inline constexpr std::string_view ENTITY = "entity";
        inline constexpr std::string_view ENTITIES = "entities";
        inline constexpr std::string_view COMPONENTS = "components";
        inline constexpr std::string_view COMPONENT_TYPES = "component_types";
        inline constexpr std::string_view IS_GLOBAL = "is_global";
        inline constexpr std::string_view IS_DEFAULT = "is_default";
        inline constexpr std::string_view ATTRIBUTES = "attributes";
        inline constexpr std::string_view CHOICES = "choices";
        inline constexpr std::string_view ICON = "icon";
        inline constexpr std::string_view ICON_COLOR = "iconColor";
        inline constexpr std::string_view VIEWPORT_ICON = "viewportIcon";
        inline constexpr std::string_view VIEWPORT_ICON_COLOR = "viewportIconColor";
    }
}
