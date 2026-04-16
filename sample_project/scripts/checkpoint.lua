-- Checkpoint (interaction trigger)
-- Demonstrates: Interaction trigger (press E), save.save_game, gamestate, log,
--               trigger.stop_timer, trigger.reset_timer, scene.create_entity,
--               entity.get_name, physics.set_transform

local activated = false

function on_create()
    log.trace("Checkpoint ready")
end

function on_update(dt)
    if activated then
        -- Gentle glow animation after activation
        local rx, ry, rz = transform.get_rotation(entity_id)
        transform.set_rotation(entity_id, rx, ry, rz + dt * 0.5)
    end
end

function on_checkpoint()
    if activated then
        log.info("Checkpoint already activated")
        return
    end
    activated = true
    log.info("Checkpoint activated! Game saved.")
    save.save_game(1)

    -- Stop the hazard timer to give the player a break (demonstrates trigger.stop_timer)
    local hazard_id = scene.find_entity("HazardTimer")
    if hazard_id ~= 0 then
        local name = entity.get_name(hazard_id)
        log.info("Disabling hazard: " .. name)
        trigger.stop_timer(hazard_id)
        trigger.reset_timer(hazard_id)
    end

    -- Snap checkpoint to grid using physics.set_transform
    local cx, cy, cz = transform.get_position(entity_id)
    physics.set_transform(entity_id, cx, cy, 0)

    -- Create a marker so other scripts know the checkpoint was reached
    scene.create_entity("CheckpointReached")
    log.info("Checkpoint marker created")
end

function on_trigger_enter()
    if not activated then
        log.info("Press E to save at checkpoint")
    end
end
