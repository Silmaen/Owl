-- Victory screen controller
-- Demonstrates: gamestate read, ui.set_text, scene.load_scene,
--               ui.transition_fade_in/out

local pending_scene = nil

function on_create()
    log.info("Victory! Game completed.")
    ui.transition_fade_in(0.5)

    -- Display final score
    local score = gamestate.get("score", 0)
    local label = scene.find_entity("ScoreLabel")
    if label ~= 0 then
        ui.set_text(label, "Final Score: " .. score)
    end
    -- Clear score for next playthrough
    gamestate.remove("score")
end

function on_update(dt)
    if pending_scene then
        if not ui.is_transition_active() then
            scene.load_scene(pending_scene)
            pending_scene = nil
        end
    end
end

function on_menu_clicked()
    pending_scene = "scenes/main_menu.owl"
    ui.transition_fade_out(0.3)
end
