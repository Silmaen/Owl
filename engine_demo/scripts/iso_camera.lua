-- Isometric test camera: the isometric layer looks at this entity's position.
-- Demonstrates: the RendererIsometric layer following the scene camera in Play,
--               panning along the screen axes of the 2:1 dimetric view.
-- The `pan` action (arrows, WASD, left stick) pans, `pause` (Escape, Start) goes back to the main menu,
-- or to the world map when entered through its house (`iso_return` in gamestate).

properties = {
    { name = "speed", type = "float", default = 6.0 },
}

local leaving = false

function on_update(dt)
    if leaving then
        return
    end
    if input.is_action_pressed("pause") then
        leaving = true
        -- A house of the world map sets `iso_return`: Escape goes back there, else to the main menu.
        local back = gamestate.get("iso_return", "")
        gamestate.remove("iso_return")
        scene.transition_to(back ~= "" and back or "scenes/main_menu.owl", "fade", 0.3)
        return
    end
    -- Screen right is world (+x, -y), screen up is world (-x, -y).
    local right, down = input.get_action_vector("pan")
    local up = -down
    if right == 0 and up == 0 then
        return
    end
    local x, y, z = transform.get_position(entity_id)
    local step = speed * dt * 0.5
    transform.set_position(entity_id, x + (right - up) * step, y + (-right - up) * step, z)
end
