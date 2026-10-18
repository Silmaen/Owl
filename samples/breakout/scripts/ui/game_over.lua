-- Game over screen of Breakout, copied from the Engine Demo (scripts/ui/game_over.lua): the score, the best one
-- (the `breakout_best` setting), Retry from level 1 or back to the title.

function on_create()
    ui.transition_fade_in(0.5)
    local score = gamestate.get("breakout_score", 0)
    local best = settings.get("breakout_best", 0)
    local text = "Score: " .. score .. "   Best: " .. best
    if score > best then
        settings.set("breakout_best", score)
        settings.save()
        text = "Score: " .. score .. "   New best!"
    end
    local label = scene.find_entity("ScoreLabel")
    if label ~= 0 then
        ui.set_text(label, text)
    end
end

function on_retry_clicked()
    gamestate.clear()
    gamestate.set("breakout_score", 0)
    gamestate.set("breakout_lives", 3)
    gamestate.set("breakout_level", 1)
    scene.transition_to("scenes/level_1.owl", "fade_out", 0.3)
end

function on_menu_clicked()
    gamestate.clear()
    scene.transition_to("scenes/title.owl", "fade_out", 0.3)
end
