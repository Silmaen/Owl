-- Victory portal - appears when every level of the hub is done (world_player.lua), leads to victory.

function on_create()
    log.trace("Teleporter standing by")
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_teleporter_enter.
function on_teleporter_enter(player_id)
    if gamestate.get("levels_done", 0) < 4 then
        return  -- the portal is asleep until every level is done
    end
    log.info("Teleporter activated! Loading victory scene.")
    -- Clear continue state — game is finished.
    gamestate.set("has_continue", false)
    gamestate.remove("continue_scene")
    scene.transition_to("scenes/victory.owl", "fade_out", 0.3)
end
