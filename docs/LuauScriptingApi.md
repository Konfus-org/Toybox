# Luau Scripting API

Toybox games are scripted in [Luau](https://luau.org). A script is an ordinary asset (a
`.luau` file) attached to a toy through a `Script` block; the engine runs it while the toy is
alive and enabled. Scripts talk to the world through the toy handle — its PascalCase
properties (`toy.Position`, `toy.Parent`), its block methods (`toy:get(UI)`), and by
querying the active scene (`tbx.sandbox:find(...)`).

## Lifecycle

Define any of these global functions; the engine calls the ones you define. Annotate the `toy`
parameter with `: Toy` to get full typing and autocomplete (the language server does not infer
it for you):

```lua
function start(toy: Toy) end                      -- once, the first frame this instance runs
function update(toy: Toy, deltaTime: number) end  -- every frame (deltaTime in seconds)
function fixedUpdate(toy: Toy, fixedDelta: number) end -- every physics step
function cleanup(toy: Toy) end                    -- once, right before the instance is unloaded
```

`toy` is a handle to the toy the script is attached to (see [Toys](#toys)). Hot-reload: editing a
`.luau` re-runs the chunk and `start` fires again for the fresh instance — `cleanup` does *not* run
on a reload (a recompile just swaps the code).

`cleanup` is the counterpart to `start` (it only runs if `start` ran) and fires once when the
script goes away: the toy is removed, its `Script` block is removed, or the app shuts down. Use it
to release what the script acquired — unsubscribe event handlers, drop external state. It fires
promptly, the frame the engine notices: if only the block was removed the toy is still alive, but
if the whole toy was removed the handle is no longer alive — so guard any toy access with
`toy.Alive`.

## Conventions

- **`:` (methods, implicit `self`)** — anything that acts on an instance you hold: a toy
  (`toy:get(...)`), a UI component (`ui:bind(...)`), the scene object (`tbx.sandbox:find(...)`).
- **`.` (properties / libraries / constants)** — toy properties (`toy.Position`), stateless
  helpers and enums: `tbx.math.add(a, b)`, `tbx.input.isDown(...)`, `tbx.Key.W`.
- **Casing** — methods are `camelCase`; toy properties, block and enum tables are `PascalCase`
  (`Position`, `Parent`, `UI`, `Key`); enum members are `UPPER_SNAKE` (`tbx.Key.W`).
- Vectors are plain tables `{ x = , y = , z = }`; quaternions likewise `{x,y,z,w}`.

## Toys

A toy is an entity. Common state is exposed as **PascalCase properties (`.`)**; blocks and
stickers go through the **generic methods (`:`)**.

### Properties (`.`)

| Property | Type | Notes |
|---|---|---|
| `toy.Alive` | bool | read-only; `false` once removed. (A handle is always truthy — use this, not `if not toy`.) |
| `toy.Name` | string | |
| `toy.Enabled` | bool | rendering + scripting + physics |
| `toy.Parent` | toy or nil | assign a toy to reparent, `nil` to detach |
| `toy.Children` | array of toys | read-only snapshot |
| `toy.Transform` | Transform or nil | the local transform block |
| `toy.Position` / `toy.Rotation` / `toy.Scale` | Vec3 / Quat / Vec3 | local-space; reading auto-creates a Transform |
| `toy.WorldPosition` / `toy.WorldRotation` / `toy.WorldScale` | Vec3 / Quat / Vec3 | read-only; composed up the parent chain |

```lua
toy.Name = "player"
toy.Position = { x = 0, y = 1, z = 0 }
if toy.Parent then print(toy.Parent.Name) end
```

### Blocks and stickers (`:`)

Blocks are identified by a **token** — `<Type>` (e.g. `UI`) — which
carries the component's type, so `get`/`add` return and type-check against it. Stickers are
plain strings.

| Method | Result | Notes |
|---|---|---|
| `toy:get(T)` | the block, or nil | typed as `T?` |
| `toy:add(T, { fields }?)` | the added block (typed) | fields optional; typed to `T` |
| `toy:add(sticker)` | toy | add a sticker, fluent |
| `toy:has(T)` / `toy:has(sticker)` | bool | block / sticker test |
| `toy:remove(T)` / `toy:remove(sticker)` | toy | detach a block / peel a sticker, fluent |
| `toy:with(...)` | toy | fluent builder: `with(sticker)`, `with(parentToy)`, `with(T, { fields }?)` |

**Custom components.** `tbx.blocks.register("Name")` mints a script-defined block and returns
its token (also exposed as a global). No C++ type, no schema, no serializer — a custom block is a
dynamic field bag you add/read like any built-in, and it lives only at runtime (not saved in
kits). Adding one returns the live fields table, so you mutate it in place:

```lua
local Ammo = tbx.blocks.register("Ammo")
function start(toy: Toy)
    toy:add(Ammo, { count = 30 })
end
-- elsewhere:
local ammo = toy:get(Ammo)
if ammo and ammo.count > 0 then ammo.count = ammo.count - 1 end
```

### Fluent verbs (`:`)

Each returns the toy, so a whole toy builds in one chain:

| Verb | Sets |
|---|---|
| `toy:parent(other)` | the parent |
| `toy:move(pos)` | local position |
| `toy:rotate(quat)` | local rotation |
| `toy:resize(scale)` | local scale |
| `toy:rename(name)` | the name |

```lua
tbx.sandbox:add("Grunt")
    :parent(hub)
    :move({ x = 0, y = 1, z = 0 })
    :add("enemy")
    :add(RigidBody, { mass = 2.0 })
```

To remove a toy, call `tbx.sandbox:remove(toy)` — a toy cannot remove itself.

## The scene — `tbx.sandbox` (`:`)

Query and mutate the active world.

| Call | Result |
|---|---|
| `tbx.sandbox:find(name)` | first toy with that name, or nil |
| `tbx.sandbox:findWith(sticker)` | array of every toy wearing the sticker |
| `tbx.sandbox:findWith(T)` | array of every toy carrying that block |
| `tbx.sandbox:getToys()` | array of every toy in the scene |
| `tbx.sandbox:add(name)` | a new empty toy |
| `tbx.sandbox:add(assetPath, position?)` | the root toy of an instantiated kit — auto-detected when the argument is an asset path (has a `/` or `.`) |
| `tbx.sandbox:remove(toy)` | remove the toy and its whole subtree |

```lua
for _, enemy in tbx.sandbox:findWith("enemy") do
    if not enemy.Alive then continue end
    -- ...
end
```

## Input — `tbx.input` (`.`)

Polled each frame. The verbs are overloaded across input kinds — pass any enum value:

- **`isDown` / `isPressed` / `isReleased`** — held / went-down-this-frame / went-up-this-frame, for a
  `tbx.Key`, a `tbx.MouseButton`, or a `tbx.GamepadButton`. For a gamepad button, pass the controller
  slot as an optional 2nd arg (default 0): `isPressed(tbx.GamepadButton.SOUTH, 1)`.
- **`getAxis(axis, slot?)`** — a `tbx.GamepadAxis` level (sticks [-1, 1], triggers [0, 1]; optional slot)
  or a `tbx.MouseAxis` (`X`/`Y` = pointer position). **`getAxisDelta(axis)`** — a `tbx.MouseAxis` frame
  delta (`X`/`Y` = movement, `SCROLL` = wheel).
- **`isGamepadConnected(slot)`**, and cursor: **`getCursorMode()` / `setCursorMode(mode)`** —
  `tbx.CursorMode.NORMAL | HIDDEN | LOCKED`.

```lua
if tbx.input.isDown(tbx.Key.W) then ... end
if tbx.input.isPressed(tbx.MouseButton.LEFT) then ... end
local lx = tbx.input.getAxis(tbx.GamepadAxis.LEFT_X, 0)   -- slot 0
local dx = tbx.input.getAxisDelta(tbx.MouseAxis.X)         -- mouse movement this frame
```

## Math — `tbx.math` (`.`)

Vector/quaternion helpers: `add`, `subtract`, `scale`, `dot`, `cross`, `length`, `distance`,
`normalize`, `lerp`, `moveToward`, `reflect`, `multiply`, `rotate`, `slerp`, `angleAxis(rad, axis)`,
`fromEuler(vec)`, `toEuler(quat)`, `quatLookAt(forward, up)`.

## Physics — `tbx.physics` (`.`)

```lua
local hit = tbx.physics.raycast(origin, direction, maxDistance)  -- nil, or:
-- hit.toy (a toy), hit.position ({x,y,z}), hit.distance (number)
```

## UI binding — `ui:bind(...)` on a UI component

Documents declare slots with `data-text` / `data-width` / `data-height` / `data-style`
attributes. Get the UI block with a token, then bind a slot to a **live getter** — the value
re-evaluates every frame, so you just mutate your own state:

```lua
local kills = 0
function start(toy: Toy)
    assert(toy:get(UI)):bind("kills", function() return kills end)
    local bar = tbx.sandbox:findWith("health_bar")[1]
    if bar then assert(bar:get(UI)):bind("health", function() return health end) end
end
```

`assert(...)` narrows the `UI?` from `get` to a non-nil `UI` (and errors if the block is
missing). Bindings live on the UI block, so two documents never collide on a slot name.

## Events — `tbx.events` (`.`)

Subscribe a handler to an engine event; it receives the event as a table:

`onInput`, `onWindowResized`, `onAssetLoaded`, `onAssetReloaded`, `onAssetUnloaded`,
`onCollision`, `onInputDeviceConnected`, `onInputDeviceDisconnected`.

Scripts are assets, so a script hot-reload arrives as `onAssetReloaded` — filter on the event's
`.extension` (`".luau"` / `".lua"`); there is no separate script-reloaded event.

```lua
tbx.events.onInputDeviceConnected(function(event)
    print("controller connected in slot " .. event.index)
end)
```

## Misc

- `tbx.quit()` — request a clean app exit.
- `print(...)` — routed through the engine log, stamped with the script's `file:line` source.

## Debugging

Scripts report through the engine log; a broken script is a log line, not a crash.

- **`print(...)`** goes to the engine log, stamped with the script's `file:line`. Chunks are named
  after the `.luau` file, so both `print` and error traces read as `player.luau:12`, not
  `[string "..."]`.
- **A hook that errors is caught and logged** as `script error in <hook>: <message>`, and the
  script keeps running — the hook is simply tried again next frame. One bad frame never hangs the
  game.
- **Compile and load errors** surface the first time the script runs (its first `start`) — sources
  are stored as plain text and compiled on first use, not at load. A syntax error logs `script
  '<name>' failed to compile: <message>`; a chunk that throws while first running logs `script
  '<name>': <message>`. Fix and save — hot-reload re-registers and the next frame's `start` runs
  the new code (state resets; `cleanup` does not run on a reload).
- **`"block is gone"` / `"block '<T>' has no field '<f>'`** — you read a block off a toy that no
  longer has it, or a field the component doesn't define. Re-check with `toy:has(T)` and
  the field name's casing.
- **A handle is always truthy** — `if not toy then …` never fires. Test liveness with `toy.Alive`
  (see [Toys](#toys)); this matters most inside `cleanup`, where a removed toy's handle is dead.
- **Type checking & autocomplete** — annotate hook params (`toy: Toy`) and the editor type-checks
  the whole script against the `tbx.*` API. Set it up with the Luau language server — see
  [LuauLanguageServer.md](LuauLanguageServer.md).

## Full example

```lua
local kills = 0

local function fire(toy: Toy, position: Vec3, forward: Vec3)
    local hit = tbx.physics.raycast(position, forward, 200.0)
    if hit and hit.toy.Alive and hit.toy:has("enemy") then
        tbx.sandbox:remove(hit.toy)
        kills = kills + 1
    end
end

function start(toy: Toy)
    assert(toy:get(UI)):bind("kills", function() return kills end)
    toy:with("player"):with(AudioListener, { volume = 1.0 })
    tbx.input.setCursorMode(tbx.CursorMode.LOCKED)
end

function update(toy: Toy, deltaTime: number)
    local position = toy.Position
    if tbx.input.isDown(tbx.Key.W) then
        position = tbx.math.add(position, { x = 0, y = 0, z = -6 * deltaTime })
    end
    toy.Position = position
    if tbx.input.isMousePressed(tbx.MouseButton.LEFT) then
        fire(toy, position, tbx.math.rotate(toy.Rotation, { x = 0, y = 0, z = -1 }))
    end
end
```
