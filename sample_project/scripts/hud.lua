-- HUD controller
-- Demonstrates: scene.find_entity
-- The health bar is driven by the player script directly.

function on_create()
    log.trace("HUD initialized")
end

function on_update(dt)
    -- HUD is passive; health bar updated by player script.
end
