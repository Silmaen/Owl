-- Raycast house door — opens the raycast E1M1-inspired demo level when the
-- player presses E nearby. Stores the world position so the player lands
-- back on the same spot when they exit the raycast scene.
--
-- Single-call transition: `scene.transition_to(path, type, duration)` lets the
-- engine drive the out-anim → loading screen → in-anim flow. No manual
-- `pending_scene` pump anymore.

function on_create()
    log.trace("RaycastHouseDoor ready")
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_raycast_door_interact.
function on_raycast_door_interact(player_id)
    local player = scene.find_entity("Player")
    if player ~= 0 then
        local px, py, pz = transform.get_position(player)
        gamestate.set("world_return_x", px)
        gamestate.set("world_return_y", py)
    end
    log.info("Entering raycast house — loading raycast demo")
    -- Wipe left so the demo shows off a non-fade variant.
    scene.transition_to("scenes/raycast_demo.owl", "wipe_left", 0.3)
end
