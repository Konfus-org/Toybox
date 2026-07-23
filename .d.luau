-- Toybox Luau API type definitions for luau-lsp (VS Code IntelliSense). Lives at the repo root
-- so it can be committed; point luau-lsp at it with the "luau-lsp.types.definitionFiles" setting
-- ([".d.luau"]) in your user or workspace settings. Reload the window after moving it.
-- Mirrors engine/src/scripting/luau/luau_bindings.cpp; keep in sync when the API changes.
-- See docs/LuauScriptingApi.md for the API and docs/LuauLanguageServer.md for setup.
-- NOTE: uses luau-lsp's current `declare extern type X with ... end` class syntax (the older
-- `declare class X ... end` form is no longer accepted by recent luau-lsp builds).

-- Plain-data value types are ordinary Lua tables (exported so scripts can annotate with them).
export type Vec2 = { x: number, y: number }
export type Vec3 = { x: number, y: number, z: number }
export type Quat = { x: number, y: number, z: number, w: number }
export type Color = { r: number, g: number, b: number, a: number }

-- A typed token identifying a block type, carrying its component type T and its constructor
-- props P as phantoms. The `tbx.blocks.<Type>` tokens have this type, so `toy:get(tbx.blocks.UI)`
-- infers `UI?` and `toy:add(tbx.blocks.UI, { document = "hud.html" })` type-checks the fields
-- against P. Opaque — the phantoms are never read at runtime (the runtime token is a small table
-- carrying the block's name).
export type BlockType<T, P> = { __block: T, __props: P }

-- Component types. Blocks are NOT toy fields — reach them with `toy:get(tbx.blocks.<Type>)`,
-- which returns the component or nil. These type declarations give `get`/`add` their return and
-- field types.
declare extern type Transform with
    position: Vec3
    rotation: Quat
    scale: Vec3
end

declare extern type UI with
    document: string
    vertex: string
    fragment: string
    mode: number
    -- Bind a document slot (data-text/-width/-style name) to a live getter; re-read each frame.
    function bind(self, slot: string, getter: () -> any): ()
end

declare extern type Camera with
    fov_degrees: number
    near_plane: number
    far_plane: number
    window: string
end

declare extern type Renderer with
    material: string
    model: string
end

declare extern type RigidBody with
    mass: number
    is_kinematic: boolean
end

declare extern type Collider with
    shape: number
    radius: number
    height: number
end

declare extern type DirectionalLight with
    color: Color
    intensity: number
end

declare extern type AudioListener with
    volume: number
end

declare extern type AudioSource with
    clip: string
    volume: number
    is_looping: boolean
    is_playing: boolean
end

declare extern type Sky with
    texture: string
    tint: Color
end

declare extern type Billboard with
    lock_y: boolean
end

declare extern type Script with
    source: string
end

-- The constructor field tables `toy:add`/`toy:with` accept per block. Every field is optional:
-- add/with default-build the block, then write whatever fields you pass. Passing no fields
-- (`toy:add(tbx.blocks.RigidBody)`) attaches a default block. These pair with each token via
-- BlockType<T, P> so `tbx.blocks.RigidBody` types its fields as RigidBodyProps.
type TransformProps = { position: Vec3?, rotation: Quat?, scale: Vec3? }
type UIProps = { document: string?, vertex: string?, fragment: string?, mode: number? }
type CameraProps = { fov_degrees: number?, near_plane: number?, far_plane: number?, window: string? }
type RendererProps = { material: string?, model: string? }
type RigidBodyProps = { mass: number?, is_kinematic: boolean? }
type ColliderProps = { shape: number?, radius: number?, height: number? }
type DirectionalLightProps = { color: Color?, intensity: number? }
type AudioListenerProps = { volume: number? }
type AudioSourceProps = { clip: string?, volume: number?, is_looping: boolean?, is_playing: boolean? }
type SkyProps = { texture: string?, tint: Color? }
type BillboardProps = { lock_y: boolean? }
type ScriptProps = { source: string? }

-- The entity handle. Common state is exposed as PascalCase properties (Roblox-style); blocks and
-- stickers go through the generic get/add/has/remove/with methods (`:`). A block is identified by
-- its `tbx.blocks.<Type>` token; a sticker by a plain string.
declare extern type Toy with
    -- Identity / hierarchy / lifecycle. Read-write unless noted. A handle is always truthy, so
    -- check `if not toy.Alive then` (not `if not toy`) to detect a removed toy.
    read Alive: boolean            -- read-only; false once the toy has been removed
    Name: string
    Enabled: boolean
    Parent: Toy?              -- nil at the root; assign a Toy to reparent, nil to detach
    Children: { Toy }         -- read-only snapshot of direct children
    Transform: Transform?     -- the local transform block (nil until one is attached)
    -- Local-space transform shortcuts (reading auto-creates a default Transform).
    Position: Vec3
    Rotation: Quat
    Scale: Vec3
    -- World-space transform, composed up the parent chain. Read-only.
    WorldPosition: Vec3
    WorldRotation: Quat
    WorldScale: Vec3

    -- Get a block, or nil when absent: `local ui = toy:get(tbx.blocks.UI)` gives `UI?`. The token
    -- carries the component type, so T is inferred — no cast.
    get: <T, P>(self: Toy, block: BlockType<T, P>) -> T?

    -- Add a block (returns the added block, typed) or a sticker (returns the toy). Fields are
    -- typed to the block: `toy:add(tbx.blocks.AudioListener, { volume = 1.0 })`.
    add: (<T, P>(self: Toy, block: BlockType<T, P>, fields: P?) -> T)
        & ((self: Toy, sticker: string) -> Toy)

    -- Test for a block (by token) or a sticker (by name).
    has: (<T, P>(self: Toy, block: BlockType<T, P>) -> boolean)
        & ((self: Toy, sticker: string) -> boolean)

    -- Remove a block (by token) or a sticker (by name). Chainable (returns the toy).
    remove: (<T, P>(self: Toy, block: BlockType<T, P>) -> Toy)
        & ((self: Toy, sticker: string) -> Toy)

    -- Fluent builder, always returns the toy: attach a block (with optional fields), add a
    -- sticker (string), or set the parent (a Toy):
    --   toy:with("player"):with(hub):with(tbx.blocks.AudioListener, { volume = 1.0 })
    with: (<T, P>(self: Toy, block: BlockType<T, P>, fields: P?) -> Toy)
        & ((self: Toy, sticker: string) -> Toy)
        & ((self: Toy, parent: Toy) -> Toy)

    -- Fluent verb shortcuts (each returns the toy), for building in one chain:
    --   sandbox:add("Grunt"):parent(hub):move({x=0,y=1,z=0}):add("enemy"):add(tbx.blocks.RigidBody)
    function parent(self, parent: Toy): Toy        -- set the parent
    function move(self, position: Vec3): Toy       -- set the local position
    function rotate(self, rotation: Quat): Toy     -- set the local rotation
    function resize(self, scale: Vec3): Toy        -- set the local scale
    function rename(self, name: string): Toy       -- set the name
end

-- To remove a toy, call sandbox:remove(toy) — a toy cannot remove itself.

export type RaycastHit = { toy: Toy, position: Vec3, distance: number }

declare tbx: {
    quit: () -> (),

    -- Typed tokens for the generic block methods: `toy:get(tbx.blocks.UI)` returns `UI?`,
    -- `toy:add(tbx.blocks.UI, { document = "hud.html" })` type-checks the fields.
    blocks: {
        Transform: BlockType<Transform, TransformProps>,
        UI: BlockType<UI, UIProps>,
        Camera: BlockType<Camera, CameraProps>,
        Renderer: BlockType<Renderer, RendererProps>,
        RigidBody: BlockType<RigidBody, RigidBodyProps>,
        Collider: BlockType<Collider, ColliderProps>,
        DirectionalLight: BlockType<DirectionalLight, DirectionalLightProps>,
        AudioListener: BlockType<AudioListener, AudioListenerProps>,
        AudioSource: BlockType<AudioSource, AudioSourceProps>,
        Sky: BlockType<Sky, SkyProps>,
        Billboard: BlockType<Billboard, BillboardProps>,
        Script: BlockType<Script, ScriptProps>,
        -- Register a custom, script-defined block and get its token (also exposed as a global).
        -- Custom blocks are dynamic field bags: no schema, script-runtime only (not serialized).
        --   local Health = tbx.blocks.register("Health")
        --   toy:add(Health, { hp = 100 });  print(toy:get(Health).hp)
        register: (name: string) -> BlockType<{ [string]: any }, { [string]: any }>,
    },

    -- The active scene: query and mutate toys (called with `:`).
    sandbox: {
        find: (self: any, name: string) -> Toy?,
        -- findWith(tag) returns all toys wearing a sticker; findWith(tbx.blocks.X) returns all
        -- toys with that block. Both return a list.
        findWith: ((self: any, sticker: string) -> { Toy })
            & (<T, P>(self: any, block: BlockType<T, P>) -> { Toy }),
        getToys: (self: any) -> { Toy },
        -- add(name) spawns an empty toy; add(assetPath, position?) instantiates a kit — it
        -- auto-deduces a kit when the argument is an asset path (has a "/" or a ".").
        add: (self: any, nameOrKit: string, position: Vec3?) -> Toy,
        -- remove(toy) removes the toy and its whole subtree.
        remove: (self: any, toy: Toy) -> (),
    },

    -- Polled input, overloaded across kinds. isDown/isPressed/isReleased take any tbx.Key /
    -- tbx.MouseButton / tbx.GamepadButton value; for a gamepad button, pass an optional
    -- controller slot (default 0). getAxis reads a tbx.GamepadAxis (with optional slot) or a
    -- tbx.MouseAxis (X/Y = pointer position); getAxisDelta reads a tbx.MouseAxis frame delta
    -- (X/Y = movement, SCROLL = wheel).
    input: {
        isDown: (input: number, slot: number?) -> boolean,
        isPressed: (input: number, slot: number?) -> boolean,
        isReleased: (input: number, slot: number?) -> boolean,
        getAxis: (axis: number, slot: number?) -> number,
        getAxisDelta: (axis: number) -> number,
        isGamepadConnected: (slot: number) -> boolean,
        getCursorMode: () -> number,
        setCursorMode: (mode: number) -> (),
    },

    math: {
        add: (a: Vec3, b: Vec3) -> Vec3,
        subtract: (a: Vec3, b: Vec3) -> Vec3,
        scale: (v: Vec3, s: number) -> Vec3,
        dot: (a: Vec3, b: Vec3) -> number,
        cross: (a: Vec3, b: Vec3) -> Vec3,
        length: (v: Vec3) -> number,
        distance: (a: Vec3, b: Vec3) -> number,
        normalize: (v: Vec3) -> Vec3,
        lerp: (a: Vec3, b: Vec3, t: number) -> Vec3,
        moveToward: (a: Vec3, b: Vec3, maxStep: number) -> Vec3,
        reflect: (v: Vec3, normal: Vec3) -> Vec3,
        multiply: (a: Quat, b: Quat) -> Quat,
        rotate: (q: Quat, v: Vec3) -> Vec3,
        slerp: (a: Quat, b: Quat, t: number) -> Quat,
        angleAxis: (radians: number, axis: Vec3) -> Quat,
        fromEuler: (radians: Vec3) -> Quat,
        toEuler: (q: Quat) -> Vec3,
        quatLookAt: (forward: Vec3, up: Vec3) -> Quat,
    },

    physics: {
        raycast: (origin: Vec3, direction: Vec3, maxDistance: number?) -> RaycastHit?,
    },

    -- Subscribe a handler to an engine event; it receives the event as a table.
    events: {
        onInput: (handler: (event: any) -> ()) -> (),
        onInputDeviceConnected: (handler: (event: { index: number }) -> ()) -> (),
        onInputDeviceDisconnected: (handler: (event: { index: number }) -> ()) -> (),
        onWindowResized: (handler: (event: any) -> ()) -> (),
        onAssetLoaded: (handler: (event: any) -> ()) -> (),
        onAssetReloaded: (handler: (event: any) -> ()) -> (),
        onAssetUnloaded: (handler: (event: any) -> ()) -> (),
        onCollision: (handler: (event: any) -> ()) -> (),
    },

    Key: {
        UNKNOWN: number,
        A: number, B: number, C: number, D: number, E: number, F: number, G: number,
        H: number, I: number, J: number, K: number, L: number, M: number, N: number,
        O: number, P: number, Q: number, R: number, S: number, T: number, U: number,
        V: number, W: number, X: number, Y: number, Z: number,
        NUM_0: number, NUM_1: number, NUM_2: number, NUM_3: number, NUM_4: number,
        NUM_5: number, NUM_6: number, NUM_7: number, NUM_8: number, NUM_9: number,
        F1: number, F2: number, F3: number, F4: number, F5: number, F6: number,
        F7: number, F8: number, F9: number, F10: number, F11: number, F12: number,
        ESCAPE: number, TAB: number, CAPS_LOCK: number, SPACE: number, ENTER: number,
        BACKSPACE: number, DEL: number, INSERT: number, HOME: number, END: number,
        PAGE_UP: number, PAGE_DOWN: number, LEFT: number, RIGHT: number, UP: number,
        DOWN: number, LEFT_SHIFT: number, RIGHT_SHIFT: number, LEFT_CTRL: number,
        RIGHT_CTRL: number, LEFT_ALT: number, RIGHT_ALT: number, MINUS: number,
        EQUALS: number, LEFT_BRACKET: number, RIGHT_BRACKET: number, BACKSLASH: number,
        SEMICOLON: number, APOSTROPHE: number, GRAVE: number, COMMA: number,
        PERIOD: number, SLASH: number,
    },
    MouseButton: { LEFT: number, RIGHT: number, MIDDLE: number },
    MouseAxis: { X: number, Y: number, SCROLL: number },
    CursorMode: { NORMAL: number, HIDDEN: number, LOCKED: number },
    GamepadButton: {
        SOUTH: number, EAST: number, WEST: number, NORTH: number, BACK: number,
        GUIDE: number, START: number, LEFT_STICK: number, RIGHT_STICK: number,
        LEFT_SHOULDER: number, RIGHT_SHOULDER: number, DPAD_UP: number, DPAD_DOWN: number,
        DPAD_LEFT: number, DPAD_RIGHT: number,
    },
    GamepadAxis: {
        LEFT_X: number, LEFT_Y: number, RIGHT_X: number, RIGHT_Y: number,
        LEFT_TRIGGER: number, RIGHT_TRIGGER: number,
    },
}

-- Block tokens are also exposed as globals, so you can drop the `tbx.blocks.` prefix:
-- `toy:get(UI)` is the same as `toy:get(tbx.blocks.UI)`. Each name is both a component type and a
-- token value (Luau keeps type and value namespaces separate).
declare Transform: BlockType<Transform, TransformProps>
declare UI: BlockType<UI, UIProps>
declare Camera: BlockType<Camera, CameraProps>
declare Renderer: BlockType<Renderer, RendererProps>
declare RigidBody: BlockType<RigidBody, RigidBodyProps>
declare Collider: BlockType<Collider, ColliderProps>
declare DirectionalLight: BlockType<DirectionalLight, DirectionalLightProps>
declare AudioListener: BlockType<AudioListener, AudioListenerProps>
declare AudioSource: BlockType<AudioSource, AudioSourceProps>
declare Sky: BlockType<Sky, SkyProps>
declare Billboard: BlockType<Billboard, BillboardProps>
declare Script: BlockType<Script, ScriptProps>

-- The engine calls these globals if you define them. ANNOTATE THE PARAMETERS YOURSELF — luau-lsp
-- does not infer them from a signature here, and `declare function`-ing them would only make them
-- "known globals" that trip BuiltinGlobalWrite / FunctionUnused when a script defines them. The
-- expected shapes (annotate to get full typing on `toy`):
--   function start(toy: Toy) end
--   function update(toy: Toy, deltaTime: number) end
--   function fixedUpdate(toy: Toy, fixedDelta: number) end
--   function cleanup(toy: Toy) end   -- before unload (toy or Script block removed, or shutdown)
