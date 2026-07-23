-- The enemy: walks toward the player on the ground plane, stopping just short.

function update(toy: Toy, deltaTime: number)
    local player = tbx.sandbox:find("Player")
    if player == nil then
        return
    end
    local at = toy.Position
    local target = player.Position
    local goal = { x = target.x, y = at.y, z = target.z }
    if tbx.math.distance(at, goal) > 1.5 then
        toy:move(tbx.math.moveToward(at, goal, 2.0 * deltaTime))
    end
end
