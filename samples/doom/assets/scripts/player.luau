-- The player: mouse look, WASD, hitscan fire, HUD, streaming focus. The whole game loop
-- lives here; the C++ side only renders and (in selftest) checks the outcome.

local yaw = 0.0
local pitch = 0.0
local kills = 0
local frame = 0
local shot_instance = nil

local function angle_axis(radians, axis)
    local half = radians * 0.5
    local s = math.sin(half)
    return { w = math.cos(half), x = axis.x * s, y = axis.y * s, z = axis.z * s }
end

local function quat_mul(a, b)
    return {
        w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    }
end

local function rotate(q, v)
    -- v + w * (2 * cross(u, v)) + cross(u, 2 * cross(u, v)), u = quat vector part
    local tx = 2.0 * (q.y * v.z - q.z * v.y)
    local ty = 2.0 * (q.z * v.x - q.x * v.z)
    local tz = 2.0 * (q.x * v.y - q.y * v.x)
    return {
        x = v.x + q.w * tx + (q.y * tz - q.z * ty),
        y = v.y + q.w * ty + (q.z * tx - q.x * tz),
        z = v.z + q.w * tz + (q.x * ty - q.y * tx),
    }
end

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

function update(toy, delta_time)
    frame = frame + 1
    local is_selftest = toy:has_sticker("selftest")

    -- The selftest ignores the mouse: the choreography must aim dead ahead regardless of
    -- what the desktop pointer does during the run.
    if not is_selftest then
        local delta = tbx.input.get_mouse_delta()
        yaw = yaw - delta.x * 0.003
        pitch = math.clamp(pitch - delta.y * 0.003, -1.4, 1.4)
    end
    local rotation = quat_mul(
        angle_axis(yaw, { x = 0.0, y = 1.0, z = 0.0 }),
        angle_axis(pitch, { x = 1.0, y = 0.0, z = 0.0 }))
    toy.Transform.rotation = rotation

    local forward = rotate(rotation, { x = 0.0, y = 0.0, z = -1.0 })
    local flat_length = math.sqrt(forward.x * forward.x + forward.z * forward.z)
    local flat = { x = forward.x / flat_length, y = 0.0, z = forward.z / flat_length }
    local right = rotate(rotation, { x = 1.0, y = 0.0, z = 0.0 })

    local position = toy.Transform.position
    local speed = 6.0 * delta_time
    if tbx.input.is_down("W") then
        position = { x = position.x + flat.x * speed, y = position.y, z = position.z + flat.z * speed }
    end
    if tbx.input.is_down("S") then
        position = { x = position.x - flat.x * speed, y = position.y, z = position.z - flat.z * speed }
    end
    if tbx.input.is_down("D") then
        position = { x = position.x + right.x * speed, y = position.y, z = position.z + right.z * speed }
    end
    if tbx.input.is_down("A") then
        position = { x = position.x - right.x * speed, y = position.y, z = position.z - right.z * speed }
    end

    -- Selftest choreography: hold still, shoot the hub enemy walking into the crosshair,
    -- then sprint north so the far room streams in.
    if is_selftest and frame > 128 then
        position = { x = position.x, y = position.y, z = position.z - 0.5 }
    end
    toy.Transform.position = position

    if tbx.input.is_pressed("ESCAPE") then
        tbx.quit()
    end
    if tbx.input.is_mouse_pressed("left") or (is_selftest and frame == 118) then
        fire(toy, position, forward)
    end

    tbx.sandbox.stream(position)
end
