local script = {}

function script.update(toy, delta_time)
    local player = tbx.sandbox.find("Player")
    if player == nil then
        return
    end
    local target = player:get("Transform").position
    local transform = toy:get("Transform")
    local at = transform.position
    local dx = target.x - at.x
    local dz = target.z - at.z
    local distance = math.sqrt(dx * dx + dz * dz)
    if distance > 1.5 then
        local step = 2.0 * delta_time / distance
        transform.position = { x = at.x + dx * step, y = at.y, z = at.z + dz * step }
    end
end

return script
