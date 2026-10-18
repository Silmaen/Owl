-- Victory screen of Wolfenstein mini: the final score, the best one (setting `wolf_best_score`), back to the title.

function on_create()
    ui.transition_fade_in(0.5)
    sound.play("sounds/ui/victory.wav")
    local score = gamestate.get("wolf_score", 0)
    if score > settings.get("wolf_best_score", 0) then
        settings.set("wolf_best_score", score)
        settings.save()
    end
    local label = scene.find_entity("ScoreLabel")
    if label ~= 0 then
        ui.set_text(label, "Final score " .. score .. "    Best " .. settings.get("wolf_best_score", 0))
    end
end

function on_menu_clicked()
    gamestate.clear()
    scene.transition_to("scenes/title.owl", "fade", 0.3)
end
