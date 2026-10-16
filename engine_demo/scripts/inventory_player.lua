-- Player of the inventory demo (top-down, like the world map).
-- Demonstrates: the built-in `move` action, a player frozen while the inventory window is open (`inv_window_open`),
-- a camera centred on the player.

properties = {
    { name = "speed", type = "float", default = 5.0 },
}

local camera_id = 0

local function follow_camera()
    if camera_id == 0 then
        return
    end
    local px, py, _ = transform.get_position(entity_id)
    local _, _, cz = transform.get_position(camera_id)
    transform.set_position(camera_id, px, py, cz)
end

function on_create()
    physics.set_gravity_scale(entity_id, 0.0)
    camera_id = scene.find_entity("MainCamera")
    follow_camera()
end

function on_update(dt)
    if ui.is_transition_active() or gamestate.get("inv_window_open", 0) == 1 then
        physics.set_velocity(entity_id, 0, 0)
        follow_camera()
        return
    end
    local dirx, diry = input.get_action_vector("move")
    physics.set_velocity(entity_id, dirx * speed, -diry * speed)
    follow_camera()
end
