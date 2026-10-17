-- Game Over screen controller
-- Demonstrates: gamestate read/clear, scene.transition_to, ui.set_text.

function on_create()
    log.info("Game Over!")
    ui.transition_fade_in(0.5)

    -- Display score
    local score = gamestate.get("score", 0)
    local label = scene.find_entity("ScoreLabel")
    if label ~= 0 then
        ui.set_text(label, "Score: " .. score)
    end
end

function on_retry_clicked()
    -- Clear state and restart from the world map.
    gamestate.clear()
    scene.transition_to("scenes/world_map.owl", "fade_out", 0.3)
end

function on_menu_clicked()
    gamestate.clear()
    scene.transition_to("scenes/main_menu.owl", "fade_out", 0.3)
end
