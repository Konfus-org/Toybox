-- The player: mouse/keyboard and controller, hitscan fire, HUD. The whole game loop lives
-- here; the C++ side only renders and (in selftest) checks the outcome. All the vector and
-- quaternion work goes through tbx.math (the engine's math library).
--
-- Controller (slot 0, alongside keyboard/mouse): left stick moves, right stick looks, A
-- fires, Start quits. Analog sticks rest near zero but jitter, so a deadzone gates them.

local GAMEPAD = 0
local STICK_DEADZONE = 0.15
local LOOK_SPEED = 2.5

local UP = { x = 0.0, y = 1.0, z = 0.0 }
local RIGHT = { x = 1.0, y = 0.0, z = 0.0 }
local AHEAD = { x = 0.0, y = 0.0, z = -1.0 }

-- A custom, script-defined component: no C++ type, no schema, no serializer — just a dynamic
-- field bag on the toy. Registered once here, then attached and read like any built-in block.
local Ammo = tbx.blocks.register("Ammo")

local yaw = 0.0
local pitch = 0.0
local kills = 0
local health = 100
local frame = 0
local shotInstance: Toy? = nil

local function deadzone(value: number): number
    if math.abs(value) < STICK_DEADZONE then
        return 0.0
    end
    return value
end

local function fire(toy: Toy, position: Vec3, forward: Vec3)
    -- Read the custom Ammo block off the toy; no shot when the magazine is empty.
    local ammo = toy:get(Ammo)
    if ammo and ammo.count <= 0 then
        return
    end
    if ammo then
        ammo.count = ammo.count - 1
    end
    local hit = tbx.physics.raycast(position, forward, 200.0)
    if hit == nil then
        return
    end
    if hit.toy.Alive and hit.toy:has("enemy") then
        tbx.sandbox:remove(hit.toy)
        kills = kills + 1
        toy:add("scored")
    end
    -- The gunshot is a kit: a speaker toy with the blip clip, placed at the hit.
    if shotInstance then
        tbx.sandbox:remove(shotInstance)
    end
    shotInstance = tbx.sandbox:add("kits/shot.kit", hit.position)
end

function start(toy: Toy)
    -- Bind HUD slots to live getters, each on its own UI component (reached through the toy
    -- hierarchy / a sticker query) so the two documents never collide on a slot name: kills on
    -- this toy's own UI (hud.html), health on the health-bar toy's UI (health.html).
    assert(toy:get(UI)):bind("kills", function() return kills end)
    -- Give the player a starting magazine on the custom Ammo block.
    toy:add(Ammo, { count = 30 })
    local healthBar = tbx.sandbox:findWith("health_bar")[1]
    if healthBar then
        assert(healthBar:get(UI)):bind("health", function() return health end)
    end

    -- Fluent build chain: attach a block and tag the toy in one expression. The player is the
    -- ears, so it carries an AudioListener.
    toy:with(AudioListener, { volume = 1.0 }):with("player")

    -- Mouse-look wants the cursor grabbed: invisible, pinned to the window, pure deltas. The
    -- selftest ignores the mouse entirely, so it leaves the real cursor alone.
    if toy:has("selftest") then
        tbx.input.setCursorMode(tbx.CursorMode.NORMAL)
    else
        tbx.input.setCursorMode(tbx.CursorMode.LOCKED)
    end

    -- React to controllers coming and going (the engine also logs these C++-side).
    tbx.events.onInputDeviceConnected(function(event)
        print("controller connected in slot " .. event.index)
    end)
    tbx.events.onInputDeviceDisconnected(function(event)
        print("controller disconnected in slot " .. event.index)
    end)
end

function update(toy: Toy, deltaTime: number)
    frame = frame + 1
    local isSelftest = toy:has("selftest")

    -- The selftest ignores the mouse: the choreography must aim dead ahead regardless of what
    -- the desktop pointer does during the run.
    if not isSelftest then
        yaw = yaw - tbx.input.getAxisDelta(tbx.MouseAxis.X) * 0.003
        pitch = math.clamp(pitch - tbx.input.getAxisDelta(tbx.MouseAxis.Y) * 0.003, -1.4, 1.4)

        -- Right stick looks; framerate-scaled so it turns at a steady rate, unlike the mouse's
        -- per-frame pixel deltas.
        local lookX = deadzone(tbx.input.getAxis(tbx.GamepadAxis.RIGHT_X, GAMEPAD))
        local lookY = deadzone(tbx.input.getAxis(tbx.GamepadAxis.RIGHT_Y, GAMEPAD))
        yaw = yaw - lookX * LOOK_SPEED * deltaTime
        pitch = math.clamp(pitch - lookY * LOOK_SPEED * deltaTime, -1.4, 1.4)
    end

    local rotation = tbx.math.multiply(
        tbx.math.angleAxis(yaw, UP),
        tbx.math.angleAxis(pitch, RIGHT))
    toy.Rotation = rotation

    local forward = tbx.math.rotate(rotation, AHEAD)
    local flat = tbx.math.normalize({ x = forward.x, y = 0.0, z = forward.z })
    local right = tbx.math.rotate(rotation, RIGHT)

    local position = toy.Position
    local speed = 6.0 * deltaTime
    if tbx.input.isDown(tbx.Key.W) then
        position = tbx.math.add(position, tbx.math.scale(flat, speed))
    end
    if tbx.input.isDown(tbx.Key.S) then
        position = tbx.math.subtract(position, tbx.math.scale(flat, speed))
    end
    if tbx.input.isDown(tbx.Key.D) then
        position = tbx.math.add(position, tbx.math.scale(right, speed))
    end
    if tbx.input.isDown(tbx.Key.A) then
        position = tbx.math.subtract(position, tbx.math.scale(right, speed))
    end

    -- Left stick moves: push-up is forward (Y reads negative up), push-right strafes.
    local moveX = deadzone(tbx.input.getAxis(tbx.GamepadAxis.LEFT_X, GAMEPAD))
    local moveY = deadzone(tbx.input.getAxis(tbx.GamepadAxis.LEFT_Y, GAMEPAD))
    position = tbx.math.add(position, tbx.math.scale(flat, -moveY * speed))
    position = tbx.math.add(position, tbx.math.scale(right, moveX * speed))

    -- Selftest choreography: hold still, shoot the hub enemy walking into the crosshair, then
    -- sprint north so the far room streams in.
    if isSelftest and frame > 128 then
        position = tbx.math.add(position, { x = 0.0, y = 0.0, z = -0.5 })
    end
    toy.Position = position

    if tbx.input.isPressed(tbx.Key.ESCAPE)
        or tbx.input.isPressed(tbx.GamepadButton.START, GAMEPAD) then
        tbx.quit()
    end
    if tbx.input.isPressed(tbx.MouseButton.LEFT)
        or tbx.input.isPressed(tbx.GamepadButton.SOUTH, GAMEPAD)
        or (isSelftest and frame == 118) then
        fire(toy, position, forward)
    end
end
