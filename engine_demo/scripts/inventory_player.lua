-- Player of the inventory demo (top-down, like the world map).
-- Demonstrates: the built-in `move` action, a player frozen while the inventory window is open (`inv_window_open`).

properties = {
    { name = "speed", type = "float", default = 5.0 },
}

function on_create()
    physics.set_gravity_scale(entity_id, 0.0)
end

function on_update(dt)
    if ui.is_transition_active() or gamestate.get("inv_window_open", 0) == 1 then
        physics.set_velocity(entity_id, 0, 0)
        return
    end
    local dirx, diry = input.get_action_vector("move")
    physics.set_velocity(entity_id, dirx * speed, -diry * speed)
end
