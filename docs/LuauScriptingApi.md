# Luau Scripting API

Toybox games are scripted in [Luau](https://luau.org). A script is an ordinary asset (a
`.luau` file) attached to a toy through a `Script` block; the engine runs it while the toy is
alive and enabled. Scripts talk to the world the way Unity C# does — through the toy hierarchy
(`toy.Transform`, `entity.UI`, `toy:getChildren()`) or by querying the active scene
(`tbx.sandbox:find(...)`).

## Lifecycle

Define any of these global functions; the engine calls the ones you define:

```lua
function start(toy) end                 -- once, the first frame this instance runs
function update(toy, deltaTime) end     -- every frame (deltaTime in seconds)
function fixedUpdate(toy, fixedDelta) end -- every physics step
```

`toy` is a handle to the toy the script is attached to (see [Toys](#toys)). Hot-reload: editing
a `.luau` re-runs the chunk; `start` fires again for each instance.

## Conventions

- **`:` (methods, implicit `self`)** — anything that acts on an instance you hold: a toy
  (`toy:setEnabled(true)`), a UI component (`ui:bind(...)`), and the scene object
  (`tbx.sandbox:find(...)`).
- **`.` (libraries / constants)** — stateless helpers and enums: `tbx.math.add(a, b)`,
  `tbx.input.isDown(...)`, `tbx.Key.W`.
- **Casing** — functions/methods are `camelCase`; component/block and enum tables are
  `PascalCase` (`Transform`, `UI`, `Key`); enum members are `UPPER_SNAKE` (`tbx.Key.W`).
- Vectors are plain tables `{ x = , y = , z = }` (w optional); quaternions likewise `{x,y,z,w}`.

## Toys

A toy is an entity. **Components are fields accessed with `.` by their PascalCase type name**;
**behaviours are methods called with `:`**.

### Components (`.`)

```lua
local pos = toy.Transform.position        -- read a component field
toy.Transform.rotation = someQuat         -- write a component field
if toy.RigidBody then ... end             -- nil when the component is absent (has-check)
toy.Health = { current = 100 }            -- add-and-populate a component
local ui = toy.UI                         -- the UI component (for :bind, below)
```

### Methods (`:`)

| Method | Result | Notes |
|---|---|---|
| `toy:getName()` | string | |
| `toy:setName(name)` | toy | fluent |
| `toy:isEnabled()` / `toy:setEnabled(b)` | bool / toy | rendering + scripting + physics |
| `toy:isAlive()` | bool | false once despawned |
| `toy:with("Type", { fields })` | toy | attach + populate a component, fluent |
| `toy:removeBlock("Type")` | toy | fluent |
| `toy:sticker(name)` | toy | slap a sticker (tag) on, fluent |
| `toy:hasSticker(name)` | bool | |
| `toy:removeSticker(name)` | toy | fluent |
| `toy:getParent()` | toy or nil | |
| `toy:getChildren()` | array of toys | direct children |
| `toy:setParent(other)` | toy | pass nil-ish to clear (a default toy) |
| `toy:despawn()` | — | removes the toy |

## The scene — `tbx.sandbox` (`:`)

Query and mutate the active world.

| Call | Result |
|---|---|
| `tbx.sandbox:find(name)` | first toy with that name, or nil |
| `tbx.sandbox:findWithSticker(sticker)` | first toy wearing the sticker, or nil |
| `tbx.sandbox:findAllWithSticker(sticker)` | array of every toy wearing the sticker |
| `tbx.sandbox:getToys()` | array of every toy in the scene |
| `tbx.sandbox:spawn(name)` | a new empty toy |
| `tbx.sandbox:spawnKit(reference, position?)` | the root toy of an instantiated kit |
| `tbx.sandbox:despawn(toy)` | remove one toy |
| `tbx.sandbox:despawnKit(toy)` | remove a toy and its whole subtree |

```lua
for _, enemy in tbx.sandbox:findAllWithSticker("enemy") do
    if not enemy:isAlive() then continue end
    -- ...
end
```

## Input — `tbx.input` (`.`)

Polled each frame. Keys/buttons/axes take strongly-typed enum values.

- Keyboard: `isDown(key)`, `isPressed(key)`, `isReleased(key)` — `tbx.Key.W`, `tbx.Key.ESCAPE`, …
- Mouse: `isMouseDown(btn)`, `isMousePressed(btn)`, `getMouseDelta()` → `{x,y}` — `tbx.MouseButton.LEFT`
- Cursor: `getCursorMode()`, `setCursorMode(mode)` — `tbx.CursorMode.NORMAL | HIDDEN | LOCKED`
- Controllers (slot 0-based): `isGamepadConnected(slot)`, `isGamepadDown(slot, btn)`,
  `isGamepadPressed(slot, btn)`, `isGamepadReleased(slot, btn)`, `getGamepadAxis(slot, axis)`
  — `tbx.GamepadButton.SOUTH`, `tbx.GamepadAxis.LEFT_X`, … (sticks in [-1, 1], triggers in [0, 1]).

```lua
if tbx.input.isDown(tbx.Key.W) then ... end
local x = tbx.input.getGamepadAxis(0, tbx.GamepadAxis.LEFT_X)
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
attributes. Bind a slot to a **live getter** on the specific UI component that owns the document
— the value re-evaluates every frame, so you just mutate your own state:

```lua
local kills = 0
function start(toy)
    toy.UI:bind("kills", function() return kills end)               -- this toy's own document
    local bar = tbx.sandbox:findWithSticker("health_bar")
    if bar then bar.UI:bind("health", function() return health end) end
end
```

Bindings live on the UI block, so two documents never collide on a slot name. For one-off writes
to the global slot table (engine-style slots) use `tbx.ui.setString(slot, value)`.

## Events — `tbx.events` (`.`)

Subscribe a handler to an engine event; it receives the event as a table:

`onKey`, `onWindowResized`, `onAssetReloaded`, `onAssetUnloaded`, `onScriptReloaded`,
`onCollision`, `onInputDeviceConnected`, `onInputDeviceDisconnected`.

```lua
tbx.events.onInputDeviceConnected(function(event)
    print("controller connected in slot " .. event.index)
end)
```

## Misc

- `tbx.quit()` — request a clean app exit.
- `print(...)` — routed through the engine log, stamped with the script's `file:line` source.

## Full example

```lua
local kills = 0

local function fire(toy, position, forward)
    local hit = tbx.physics.raycast(position, forward, 200.0)
    if hit and hit.toy:isAlive() and hit.toy:hasSticker("enemy") then
        hit.toy:despawn()
        kills = kills + 1
    end
end

function start(toy)
    toy.UI:bind("kills", function() return kills end)
    toy:sticker("player")
    tbx.input.setCursorMode(tbx.CursorMode.LOCKED)
end

function update(toy, deltaTime)
    local position = toy.Transform.position
    if tbx.input.isDown(tbx.Key.W) then
        position = tbx.math.add(position, { x = 0, y = 0, z = -6 * deltaTime })
    end
    toy.Transform.position = position
    if tbx.input.isMousePressed(tbx.MouseButton.LEFT) then
        fire(toy, position, tbx.math.rotate(toy.Transform.rotation, { x = 0, y = 0, z = -1 }))
    end
end
```
