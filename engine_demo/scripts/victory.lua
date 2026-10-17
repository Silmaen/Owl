-- Victory screen controller
-- Demonstrates: gamestate read, ui.set_text, scene.transition_to.

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

function on_menu_clicked()
    scene.transition_to("scenes/main_menu.owl", "fade_out", 0.3)
end
