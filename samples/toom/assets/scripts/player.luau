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

local yaw = 0.0
local pitch = 0.0
local kills = 0
local health = 100
local frame = 0
local shotInstance = nil

local function deadzone(value: number): number
    if math.abs(value) < STICK_DEADZONE then
        return 0.0
    end
    return value
end

local function fire(toy, position, forward)
    local hit = tbx.physics.raycast(position, forward, 200.0)
    if hit == nil then
        return
    end
    if hit.toy and hit.toy:isAlive() and hit.toy:hasSticker("enemy") then
        hit.toy:despawn()
        kills = kills + 1
        toy:sticker("scored")
    end
    -- The gunshot is a kit: a speaker toy with the blip clip, placed at the hit.
    if shotInstance then
        tbx.sandbox:despawnKit(shotInstance)
    end
    shotInstance = tbx.sandbox:spawnKit("kits/shot.kit", hit.position)
end

function start(toy)
    -- Bind HUD slots to live getters, each on its own UI component (reached through the toy
    -- hierarchy / a sticker query) so the two documents never collide on a slot name: kills on
    -- this toy's own UI (hud.html), health on the health-bar toy's UI (health.html).
    toy.UI:bind("kills", function() return kills end)
    local healthBar = tbx.sandbox:findWithSticker("health_bar")
    if healthBar then
        healthBar.UI:bind("health", function() return health end)
    end

    -- Fluent build chain: attach a block and tag the toy in one expression. The player is the
    -- ears, so it carries an AudioListener.
    toy:with("AudioListener", { volume = 1.0 }):sticker("player")

    -- Mouse-look wants the cursor grabbed: invisible, pinned to the window, pure deltas. The
    -- selftest ignores the mouse entirely, so it leaves the real cursor alone.
    if toy:hasSticker("selftest") then
        tbx.input.setCursorMode(tbx.CursorMode.NORMAL)
    else
        tbx.input.setCursorMode(tbx.CursorMode.LOCKED)
    end

    -- React to controllers coming and going (the engine also logs these C++-side).
    tbx.events.onInputDeviceConnected(function(event)
        print("player: controller connected in slot " .. event.index)
    end)
    tbx.events.onInputDeviceDisconnected(function(event)
        print("player: controller disconnected in slot " .. event.index)
    end)
end

function update(toy, deltaTime: number)
    frame = frame + 1
    local isSelftest = toy:hasSticker("selftest")

    -- The selftest ignores the mouse: the choreography must aim dead ahead regardless of what
    -- the desktop pointer does during the run.
    if not isSelftest then
        local delta = tbx.input.getMouseDelta()
        yaw = yaw - delta.x * 0.003
        pitch = math.clamp(pitch - delta.y * 0.003, -1.4, 1.4)

        -- Right stick looks; framerate-scaled so it turns at a steady rate, unlike the mouse's
        -- per-frame pixel deltas.
        local lookX = deadzone(tbx.input.getGamepadAxis(GAMEPAD, tbx.GamepadAxis.RIGHT_X))
        local lookY = deadzone(tbx.input.getGamepadAxis(GAMEPAD, tbx.GamepadAxis.RIGHT_Y))
        yaw = yaw - lookX * LOOK_SPEED * deltaTime
        pitch = math.clamp(pitch - lookY * LOOK_SPEED * deltaTime, -1.4, 1.4)
    end

    local rotation = tbx.math.multiply(
        tbx.math.angleAxis(yaw, UP),
        tbx.math.angleAxis(pitch, RIGHT))
    toy.Transform.rotation = rotation

    local forward = tbx.math.rotate(rotation, AHEAD)
    local flat = tbx.math.normalize({ x = forward.x, y = 0.0, z = forward.z })
    local right = tbx.math.rotate(rotation, RIGHT)

    local position = toy.Transform.position
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
    local moveX = deadzone(tbx.input.getGamepadAxis(GAMEPAD, tbx.GamepadAxis.LEFT_X))
    local moveY = deadzone(tbx.input.getGamepadAxis(GAMEPAD, tbx.GamepadAxis.LEFT_Y))
    position = tbx.math.add(position, tbx.math.scale(flat, -moveY * speed))
    position = tbx.math.add(position, tbx.math.scale(right, moveX * speed))

    -- Selftest choreography: hold still, shoot the hub enemy walking into the crosshair, then
    -- sprint north so the far room streams in.
    if isSelftest and frame > 128 then
        position = tbx.math.add(position, { x = 0.0, y = 0.0, z = -0.5 })
    end
    toy.Transform.position = position

    if tbx.input.isPressed(tbx.Key.ESCAPE)
        or tbx.input.isGamepadPressed(GAMEPAD, tbx.GamepadButton.START) then
        tbx.quit()
    end
    if tbx.input.isMousePressed(tbx.MouseButton.LEFT)
        or tbx.input.isGamepadPressed(GAMEPAD, tbx.GamepadButton.SOUTH)
        or (isSelftest and frame == 118) then
        fire(toy, position, forward)
    end
end
