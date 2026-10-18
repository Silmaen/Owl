-- Victory screen of Wolfenstein mini: the final score, the best one (setting `wolf_best_score`), back to the title.

function on_create()
    if not ui.is_transition_active() then ui.transition_play("fade_in", 0.5, 0.22, 0.02, 0.02) end
    sound.play("sounds/ui/victory.wav")
    local score = gamestate.get("wolf_score", 0)
    if score > settings.get("wolf_best_score", 0) then
        settings.set("wolf_best_score", score)
        settings.save()
    end
    local label = scene.find_entity("ScoreLabel")
    if label ~= 0 then
        ui.set_text(label, "FINAL SCORE " .. score .. "     BEST " .. settings.get("wolf_best_score", 0))
    end
end

function on_menu_clicked()
    gamestate.clear()
    scene.transition_to("scenes/title.owl", "fade_out", 0.4, 0.22, 0.02, 0.02)
end
