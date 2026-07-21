-- The enemy: walks toward the player on the ground plane, stopping just short.

function update(toy, delta_time)
    local player = tbx.sandbox.find("Player")
    if player == nil then
        return
    end
    local at = toy.Transform.position
    local target = player.Transform.position
    local goal = { x = target.x, y = at.y, z = target.z }
    if tbx.math.distance(at, goal) > 1.5 then
        toy.Transform.position = tbx.math.move_toward(at, goal, 2.0 * delta_time)
    end
end
