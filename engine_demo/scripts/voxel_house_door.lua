-- Voxel house door — opens the procedural voxel terrain when the player presses E
-- nearby. Stores the world position so the player can be returned to the same
-- spot later. Mirrors raycast_house_door.lua but targets the voxel terrain.

function on_create()
    log.trace("VoxelHouseDoor ready")
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_voxel_door_interact.
function on_voxel_door_interact(player_id)
    local player = scene.find_entity("Player")
    if player ~= 0 then
        local px, py, pz = transform.get_position(player)
        gamestate.set("world_return_x", px)
        gamestate.set("world_return_y", py)
    end
    log.info("Entering voxel house — loading voxel terrain")
    scene.transition_to("scenes/voxel_terrain.owl", "fade", 0.3)
end
