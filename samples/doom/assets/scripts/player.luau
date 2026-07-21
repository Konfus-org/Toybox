-- The player: mouse look, WASD, hitscan fire, HUD, streaming focus. The whole game loop
-- lives here; the C++ side only renders and (in selftest) checks the outcome. All the vector
-- and quaternion work goes through tbx.math (the engine's math library).

local yaw: number = 0.0
local pitch: number = 0.0
local kills: number = 0
local frame: number = 0
local shot_instance = nil

local UP = { x = 0.0, y = 1.0, z = 0.0 }
local RIGHT = { x = 1.0, y = 0.0, z = 0.0 }
local AHEAD = { x = 0.0, y = 0.0, z = -1.0 }

local function fire(toy, position, forward)
    local hit = tbx.physics.raycast(position, forward, 200.0)
    if hit == nil then
        return
    end
    if hit.toy and hit.toy:is_alive() and hit.toy:has_sticker("enemy") then
        tbx.sandbox.despawn(hit.toy)
        kills = kills + 1
        toy:sticker("scored")
        tbx.ui.set_style("kills", string.format("width: %dpx;", kills * 40))
    end
    -- The gunshot is a kit: a speaker toy with the blip clip, placed at the hit.
    if shot_instance then
        tbx.sandbox.despawn_kit(shot_instance)
    end
    shot_instance = tbx.sandbox.spawn_kit("kits/shot.kit", hit.position)
end

function update(toy, delta_time: number)
    frame = frame + 1
    local is_selftest = toy:has_sticker("selftest")

    -- The selftest ignores the mouse: the choreography must aim dead ahead regardless of
    -- what the desktop pointer does during the run.
    if not is_selftest then
        local delta = tbx.input.get_mouse_delta()
        yaw = yaw - delta.x * 0.003
        pitch = math.clamp(pitch - delta.y * 0.003, -1.4, 1.4)
    end
    local rotation = tbx.math.multiply(
        tbx.math.angle_axis(yaw, UP),
        tbx.math.angle_axis(pitch, RIGHT))
    toy.Transform.rotation = rotation

    local forward = tbx.math.rotate(rotation, AHEAD)
    local flat = tbx.math.normalize({ x = forward.x, y = 0.0, z = forward.z })
    local right = tbx.math.rotate(rotation, RIGHT)

    local position = toy.Transform.position
    local speed = 6.0 * delta_time
    if tbx.input.is_down(tbx.Key.W) then
        position = tbx.math.add(position, tbx.math.scale(flat, speed))
    end
    if tbx.input.is_down(tbx.Key.S) then
        position = tbx.math.subtract(position, tbx.math.scale(flat, speed))
    end
    if tbx.input.is_down(tbx.Key.D) then
        position = tbx.math.add(position, tbx.math.scale(right, speed))
    end
    if tbx.input.is_down(tbx.Key.A) then
        position = tbx.math.subtract(position, tbx.math.scale(right, speed))
    end

    -- Selftest choreography: hold still, shoot the hub enemy walking into the crosshair,
    -- then sprint north so the far room streams in.
    if is_selftest and frame > 128 then
        position = tbx.math.add(position, { x = 0.0, y = 0.0, z = -0.5 })
    end
    toy.Transform.position = position

    if tbx.input.is_pressed(tbx.Key.ESCAPE) then
        tbx.quit()
    end
    if tbx.input.is_mouse_pressed(tbx.MouseButton.LEFT) or (is_selftest and frame == 118) then
        fire(toy, position, forward)
    end

    tbx.sandbox.stream(position)
end
