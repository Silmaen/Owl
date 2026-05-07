-- Teleporter — appears when every house has been explored, leads to victory.

function on_create()
    log.trace("Teleporter standing by")
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_teleporter_enter.
function on_teleporter_enter(player_id)
    log.info("Teleporter activated! Loading victory scene.")
    -- Clear continue state — game is finished.
    gamestate.set("has_continue", false)
    gamestate.remove("continue_scene")
    scene.transition_to("scenes/victory.owl", "fade_out", 0.3)
end
