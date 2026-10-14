-- Isometric test camera: the isometric layer looks at this entity's position.
-- Demonstrates: the RendererIsometric layer following the scene camera in Play,
--               panning along the screen axes of the 2:1 dimetric view.
-- Arrows / WASD pan, Escape goes back to the main menu.

properties = {
    { name = "speed", type = "float", default = 6.0 },
}

local leaving = false

function on_update(dt)
    if leaving then
        return
    end
    if input.is_key_pressed(256) then
        leaving = true
        scene.transition_to("scenes/main_menu.owl", "fade", 0.3)
        return
    end
    -- Screen right is world (+x, -y), screen up is world (-x, -y).
    local right = 0
    local up = 0
    if input.is_key_pressed(262) or input.is_key_pressed(68) then right = right + 1 end
    if input.is_key_pressed(263) or input.is_key_pressed(65) then right = right - 1 end
    if input.is_key_pressed(265) or input.is_key_pressed(87) then up = up + 1 end
    if input.is_key_pressed(264) or input.is_key_pressed(83) then up = up - 1 end
    if right == 0 and up == 0 then
        return
    end
    local x, y, z = transform.get_position(entity_id)
    local step = speed * dt * 0.5
    transform.set_position(entity_id, x + (right - up) * step, y + (-right - up) * step, z)
end
