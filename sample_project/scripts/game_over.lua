-- Game Over screen controller
-- Demonstrates: gamestate read/clear, scene.load_scene, ui.set_text,
--               ui.transition_fade_in/out

local pending_scene = nil

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

function on_update(dt)
    if pending_scene then
        if not ui.is_transition_active() then
            scene.load_scene(pending_scene)
            pending_scene = nil
        end
    end
end

function on_retry_clicked()
    -- Clear state and restart gameplay
    gamestate.clear()
    pending_scene = "scenes/gameplay.owl"
    ui.transition_fade_out(0.3)
end

function on_menu_clicked()
    gamestate.clear()
    pending_scene = "scenes/main_menu.owl"
    ui.transition_fade_out(0.3)
end
