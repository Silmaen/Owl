-- Teleporter — appears when every house has been explored, leads to victory.

local pending_scene = nil

function on_create()
    log.trace("Teleporter standing by")
end

function on_update(dt)
    if pending_scene and not ui.is_transition_active() then
        scene.load_scene(pending_scene)
        pending_scene = nil
    end
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_teleporter_enter.
function on_teleporter_enter(player_id)
    log.info("Teleporter activated! Loading victory scene.")
    -- Clear continue state — game is finished.
    gamestate.set("has_continue", false)
    gamestate.remove("continue_scene")
    pending_scene = "scenes/victory.owl"
    ui.transition_fade_out(0.3)
end
