-- Iso town house door — opens the isometric town when the player presses E nearby.
-- Remembers the world position and asks iso_camera.lua to come back to the world map on Escape.

function on_create()
    log.trace("IsoHouseDoor ready")
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_iso_door_interact.
function on_iso_door_interact(player_id)
    local player = scene.find_entity("Player")
    if player ~= 0 then
        local px, py, pz = transform.get_position(player)
        gamestate.set("world_return_x", px)
        gamestate.set("world_return_y", py)
    end
    gamestate.set("iso_return", "scenes/world_map.owl")
    log.info("Entering iso town house — loading the isometric town")
    scene.transition_to("scenes/iso_town.owl", "fade", 0.3)
end
