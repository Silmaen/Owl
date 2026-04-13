-- HUD controller
-- Demonstrates: ui.set_progress, scene.find_entity

local health_bar_id = 0
local health = 1.0

function on_create()
    health_bar_id = scene.find_entity("HealthBar")
end

function on_update(dt)
    -- Slowly decrease health bar for visual demo
    if health_bar_id ~= 0 then
        health = health - dt * 0.05
        if health < 0 then health = 1.0 end
        ui.set_progress(health_bar_id, health)
    end
end
