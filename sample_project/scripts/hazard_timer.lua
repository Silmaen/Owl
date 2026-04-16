-- Hazard Timer
-- Demonstrates: Timer trigger type (repeating), log.warn, transform animation,
--               trigger.start_timer (restarts after being stopped by checkpoint)

local pulse = 0
local tick_count = 0

function on_create()
    log.info("Hazard timer started (repeating)")
end

function on_update(dt)
    -- Visual pulse effect
    pulse = pulse + dt * 3
    local scale = 1.0 + math.sin(pulse) * 0.1
    transform.set_scale(entity_id, scale, scale, 1)

    -- If checkpoint was reached and timer was stopped, restart it
    local marker = scene.find_entity("CheckpointReached")
    if marker ~= 0 then
        trigger.start_timer(entity_id)
        log.info("Hazard timer restarted after checkpoint break")
        scene.destroy_entity(marker)
    end
end

function on_timer()
    tick_count = tick_count + 1
    log.warn("Hazard pulse #" .. tick_count .. "! Danger nearby.")
end
