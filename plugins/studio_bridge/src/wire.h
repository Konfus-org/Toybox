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
        // Dormant: no Studio 2.0 call site yet; kept for the asset browser's texture previews.
        inline constexpr std::string_view EDITOR_PREVIEW_TEXTURE_MATERIAL =
            "editor.previewTextureMaterial";
        inline constexpr std::string_view EDITOR_LOG = "editor.log";
        // The editor's render-layers toolbar: collider wireframe modes, the post-processing toggle,
        // and the render-stage debug view, pushed as one notification (see render_layers_ops).
        inline constexpr std::string_view EDITOR_SET_RENDER_LAYERS = "editor.setRenderLayers";

        inline constexpr std::string_view ENGINE_PING = "engine.ping";
        inline constexpr std::string_view ENGINE_SHUTDOWN = "engine.shutdown";
        inline constexpr std::string_view ENGINE_SET_PAUSED = "engine.setPaused";
        inline constexpr std::string_view ENGINE_SET_PLAYING = "engine.setPlaying";
        inline constexpr std::string_view ENGINE_SET_LOG_COLORS = "engine.setLogColors";
        inline constexpr std::string_view ENGINE_LOG = "engine.log";

        // Dormant: no Studio 2.0 call site yet; kept for world opening/saving from the editor.
        inline constexpr std::string_view WORLD_DESCRIBE = "world.describe";
        inline constexpr std::string_view WORLD_SAVE = "world.save";
        inline constexpr std::string_view WORLD_OPEN = "world.open";

        // entity.create/describe/move/removeComponent/addScript are dormant: no Studio 2.0 call
        // site yet; kept for the world tree's structural editing.
        inline constexpr std::string_view ENTITY_CREATE = "entity.create";
        inline constexpr std::string_view ENTITY_DESCRIBE = "entity.describe";
        inline constexpr std::string_view ENTITY_DESTROY = "entity.destroy";
        inline constexpr std::string_view ENTITY_MOVE = "entity.move";
        inline constexpr std::string_view ENTITY_ADD_COMPONENT = "entity.addComponent";
        inline constexpr std::string_view ENTITY_REMOVE_COMPONENT = "entity.removeComponent";
        inline constexpr std::string_view ENTITY_ADD_SCRIPT = "entity.addScript";

        inline constexpr std::string_view ASSET_DESCRIBE = "asset.describe";
        inline constexpr std::string_view ASSET_SAVE = "asset.save";
        inline constexpr std::string_view ASSET_CREATE = "asset.create";
        // asset.describe (above) and forget/newId/previewStats/pairing/generateMissingMetas are
        // dormant: no Studio 2.0 call site yet; kept for the asset browser's pipeline tooling.
        inline constexpr std::string_view ASSET_FORGET = "asset.forget";
        inline constexpr std::string_view ASSET_NEW_ID = "asset.newId";
        inline constexpr std::string_view ASSET_PREVIEW_STATS = "asset.previewStats";
        inline constexpr std::string_view ASSET_PAIRING = "asset.pairing";
        inline constexpr std::string_view ASSET_GENERATE_MISSING_METAS =
            "asset.generateMissingMetas";

        inline constexpr std::string_view SYNC_DESCRIBE = "sync.describe";
        // Transitional: retired once Studio 2.0's family write verbs (component.set / entity.set /
        // asset.set) land on the same path-addressed set machinery.
        inline constexpr std::string_view SYNC_SET = "sync.set";
        inline constexpr std::string_view SYNC_RESET = "sync.reset";
        inline constexpr std::string_view SYNC_IS_DEFAULT = "sync.isDefault";
        // The sync.event channel: the editor subscribes (address, key) pairs; the engine streams the
        // matching raises back as sync.event notifications (see sync_event_ops).
        inline constexpr std::string_view SYNC_SUBSCRIBE = "sync.subscribe";
        inline constexpr std::string_view SYNC_UNSUBSCRIBE = "sync.unsubscribe";
        inline constexpr std::string_view SYNC_EVENT = "sync.event";
        // Engine-to-editor notification: one synced value changed engine-side ({address, key, value}
        // in the editor's wire shapes); the editor's sync hub applies it to the bound mirror. Sent by
        // the gizmo controller when a drag lands on an entity's Transform.
        inline constexpr std::string_view SYNC_CHANGED = "sync.changed";

        // The editor's selection push (Ecs/WorldSelection): engine-global {key: "ids", value: [...]}.
        inline constexpr std::string_view SELECTION_SET = "selection.set";

        // The runtime physics surface: raycast is a reply-carrying query, overlapScan a manual trigger
        // scan request by component address. The trigger/collider raises ride the sync.event channel.
        inline constexpr std::string_view PHYSICS_RAYCAST = "physics.raycast";
        inline constexpr std::string_view PHYSICS_OVERLAP_SCAN = "physics.overlapScan";

        inline constexpr std::string_view VIEW_START = "view.start";
        inline constexpr std::string_view VIEW_STOP = "view.stop";
        inline constexpr std::string_view VIEW_INPUT = "view.input";
        inline constexpr std::string_view VIEW_PICK = "view.pick";
        // view.pickRect/projectEntities/queryOcclusion/frameAssetPreview are dormant: no Studio 2.0
        // call site yet; kept for box-select, the billboard/icon overlays (which batch projection
        // and ride the occlusion query), and asset-preview framing.
        inline constexpr std::string_view VIEW_PICK_RECT = "view.pickRect";
        inline constexpr std::string_view VIEW_PROJECT_ENTITIES = "view.projectEntities";
        inline constexpr std::string_view VIEW_QUERY_OCCLUSION = "view.queryOcclusion";
        inline constexpr std::string_view VIEW_SET_GIZMO = "view.setGizmo";
        inline constexpr std::string_view VIEW_FRAME_ASSET_PREVIEW = "view.frameAssetPreview";
        // Engine-to-editor notifications (sent through the RPC host, never registered).
        inline constexpr std::string_view VIEW_SURFACE = "view.surface";
        inline constexpr std::string_view VIEW_PRESENTED = "view.presented";
        // Dormant notification: nothing consumes it yet; kept for the game panel's cursor capture.
        inline constexpr std::string_view INPUT_MOUSE_LOCK = "input.mouseLock";

        // The editor-authored gizmo overlay: retained named layers of drawing ops the layer store
        // replays into the engine's gizmo renderer over editor viewports.
        inline constexpr std::string_view GIZMOS_SET = "gizmos.set";
        inline constexpr std::string_view GIZMOS_REMOVE = "gizmos.remove";

        // --- Recurring JSON param/reply keys ---

        // Addressing: which world/entity/asset/component/property (or sync path) an op targets.
        inline constexpr std::string_view WORLD_ASSET_ID = "worldAssetId";
        inline constexpr std::string_view ENTITY_ID = "entityId";
        inline constexpr std::string_view ASSET_ID = "assetId";
        inline constexpr std::string_view COMPONENT = "component";
        inline constexpr std::string_view PROPERTY = "property";
        inline constexpr std::string_view PATH = "path";
        inline constexpr std::string_view VIEW = "view";
        // The engine-sync family commands' uniform payload: which object, which value, the value.
        inline constexpr std::string_view ADDRESS = "address";
        inline constexpr std::string_view KEY = "key";

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
        // The input.mouseLock notification's payload (the game's cursor-capture mode name).
        inline constexpr std::string_view MODE = "mode";
        inline constexpr std::string_view FORMAT = "format";
        inline constexpr std::string_view SETTINGS = "settings";
        inline constexpr std::string_view LEVEL = "level";
        inline constexpr std::string_view MESSAGE = "message";
        inline constexpr std::string_view OCCLUDED = "occluded";
        // A gizmo layer's synced values (camelCase, the editor's generated wire keys).
        inline constexpr std::string_view OPS = "ops";
        inline constexpr std::string_view IS_VISIBLE = "isVisible";
        // The transform-gizmo params (view.setGizmo): the editor-pushed handle set + snap settings.
        inline constexpr std::string_view HANDLES = "handles";
        inline constexpr std::string_view KIND = "kind";
        inline constexpr std::string_view AXIS = "axis";
        inline constexpr std::string_view EXTENT = "extent";
        inline constexpr std::string_view SNAP = "snap";
        inline constexpr std::string_view TRANSLATE = "translate";
        inline constexpr std::string_view ROTATE_DEG = "rotateDeg";
        inline constexpr std::string_view KEYS = "keys";
        // The view.pick reply's handle-tap flag: the cursor was on a gizmo handle, so neither select
        // nor clear.
        inline constexpr std::string_view GIZMO = "gizmo";
        // The editor.setRenderLayers params: the collider wireframe modes, the post toggle, and the
        // render-stage name ("final"/"diffuse"/"normals"/"shadows"/"depth").
        inline constexpr std::string_view COLLIDERS_ALL = "collidersAll";
        inline constexpr std::string_view COLLIDERS_SELECTED = "collidersSelected";
        inline constexpr std::string_view POST_PROCESSING = "postProcessing";
        inline constexpr std::string_view STAGE = "stage";
        // The Transform mirror's synced value keys (the sync.changed reflow after a gizmo drag);
        // the third, POSITION, lives with the physics contact fields below.
        inline constexpr std::string_view ROTATION = "rotation";
        inline constexpr std::string_view SCALE = "scale";
        // The sync.event notification payload: the raise's args ride beside address/key.
        inline constexpr std::string_view ARGS = "args";
        // The physics event keys (the editor's camelCase event member names) and their args fields.
        inline constexpr std::string_view EVENT_OVERLAP_BEGAN = "overlapBegan";
        inline constexpr std::string_view EVENT_OVERLAP_STAYED = "overlapStayed";
        inline constexpr std::string_view EVENT_OVERLAP_ENDED = "overlapEnded";
        inline constexpr std::string_view EVENT_COLLISION_BEGAN = "collisionBegan";
        inline constexpr std::string_view EVENT_COLLISION_ENDED = "collisionEnded";
        inline constexpr std::string_view TRIGGER = "trigger";
        inline constexpr std::string_view OTHER = "other";
        // The physics.raycast params/reply and the contact args' spatial fields.
        inline constexpr std::string_view ORIGIN = "origin";
        inline constexpr std::string_view DIRECTION = "direction";
        inline constexpr std::string_view MAX_DISTANCE = "maxDistance";
        inline constexpr std::string_view IGNORE_ENTITY_ID = "ignoreEntityId";
        inline constexpr std::string_view HAS_HIT = "hasHit";
        inline constexpr std::string_view POSITION = "position";
        inline constexpr std::string_view NORMAL = "normal";
        inline constexpr std::string_view FRACTION = "fraction";
        // The reply key of sync.isDefault (distinct from the describe shape's "is_default" below).
        inline constexpr std::string_view IS_DEFAULT_REPLY = "isDefault";

        // The describe payload shape: entity bodies and per-field wrappers.
        inline constexpr std::string_view ENTITY = "entity";
        inline constexpr std::string_view ENTITIES = "entities";
        inline constexpr std::string_view COMPONENTS = "components";
        inline constexpr std::string_view IS_GLOBAL = "is_global";
        inline constexpr std::string_view IS_DEFAULT = "is_default";
        inline constexpr std::string_view ATTRIBUTES = "attributes";
        inline constexpr std::string_view CHOICES = "choices";
    }
}
