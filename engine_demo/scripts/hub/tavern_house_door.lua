-- Tavern house door — opens the tavern, a 3D level, when the player presses E
-- nearby. Stores the world position so the player can be returned to the same
-- spot later. Mirrors voxel_house_door.lua but targets the tavern.

function on_create()
    log.trace("TavernHouseDoor ready")
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_tavern_door_interact.
function on_tavern_door_interact(player_id)
    local player = scene.find_entity("Player")
    if player ~= 0 then
        local px, py, pz = transform.get_position(player)
        gamestate.set("world_return_x", px)
        gamestate.set("world_return_y", py)
    end
    log.info("Entering the tavern")
    sound.play("sounds/hub/door.wav")
    scene.transition_to("scenes/tavern.owl", "fade", 0.3)
end
