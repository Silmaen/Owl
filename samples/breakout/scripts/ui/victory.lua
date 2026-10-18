-- Victory screen of Breakout, copied from the Engine Demo (scripts/ui/victory.lua): the final score and the best one,
-- kept as the `breakout_best` setting.

function on_create()
    ui.transition_fade_in(0.5)
    sound.play("sounds/ui/victory.wav")
    local score = gamestate.get("breakout_score", 0)
    local best = settings.get("breakout_best", 0)
    local text = "Final Score: " .. score .. "   Best: " .. best
    if score > best then
        settings.set("breakout_best", score)
        settings.save()
        text = "Final Score: " .. score .. "   New best!"
    end
    local label = scene.find_entity("ScoreLabel")
    if label ~= 0 then
        ui.set_text(label, text)
    end
end

function on_menu_clicked()
    gamestate.clear()
    scene.transition_to("scenes/title.owl", "fade_out", 0.3)
end
