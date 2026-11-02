-- Platformer player: the built-in `move` action (A / D or left stick) runs, `jump` (space or A) jumps from the ground.
-- The Box2D body is the source of truth: move it through `physics`, never `transform`.

properties = {
    {name = "speed", type = "float", default = 5.0},
    {name = "jump_speed", type = "float", default = 7.5},
}

function on_update(dt)
    local mx, _ = input.get_action_vector("move")
    local _, vy = physics.get_velocity(entity_id)
    if input.is_action_just_pressed("jump") and math.abs(vy) < 0.01 then
        vy = jump_speed
    end
    physics.set_velocity(entity_id, mx * speed, vy)
end
