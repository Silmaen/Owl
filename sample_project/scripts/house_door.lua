-- House door — opens the platformer level when the player presses E nearby.
-- Once the level has been completed, the door reports as already explored
-- and the gamestate flag `houses_visited` is bumped (handled by level_complete).

local door_index = 1  -- this is house #1 in the sample (only one for now)
local pending_scene = nil

function on_create()
    log.trace("HouseDoor #" .. door_index .. " ready")
end

function on_update(dt)
    if pending_scene and not ui.is_transition_active() then
        scene.load_scene(pending_scene)
        pending_scene = nil
    end
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_door_interact.
function on_door_interact(player_id)
    local key = "house_" .. door_index .. "_done"
    if gamestate.get(key, 0) > 0 then
        log.info("This house has already been explored.")
        return
    end
    -- Save the current world position so we land back at the same spot when
    -- the platformer level completes.
    local player = scene.find_entity("Player")
    if player ~= 0 then
        local px, py, pz = transform.get_position(player)
        gamestate.set("world_return_x", px)
        gamestate.set("world_return_y", py)
    end
    gamestate.set("active_door", door_index)
    log.info("Entering house — loading platformer scene")
    pending_scene = "scenes/platformer_house.owl"
    ui.transition_fade_out(0.3)
end
