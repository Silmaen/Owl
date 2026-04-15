-- Portal entity script
-- Demonstrates: on_trigger_enter, sound.play, visual animation,
--               transform.get_scale, physics.get_velocity, physics.set_velocity

local pulse = 0
local base_sx = 1
local base_sy = 1.5

function on_create()
    -- Read initial scale (demonstrates transform.get_scale)
    base_sx, base_sy, _ = transform.get_scale(entity_id)
    log.trace("Portal ready, base scale: " .. base_sx .. "x" .. base_sy)
end

function on_update(dt)
    -- Pulsing glow effect
    pulse = pulse + dt * 2
    local s = 1.0 + math.sin(pulse) * 0.15
    transform.set_scale(entity_id, base_sx * s, base_sy * s, 1)
end

function on_trigger_enter()
    sound.play("sounds/teleport.wav")
    log.info("Entering portal!")

    -- Slow down the player on portal entry (demonstrates physics.get/set_velocity)
    local player_id = scene.find_entity("Player")
    if player_id ~= 0 then
        local vx, vy = physics.get_velocity(player_id)
        log.trace("Player velocity at portal: " .. vx .. ", " .. vy)
        physics.set_velocity(player_id, vx * 0.2, vy * 0.2)
    end
end
