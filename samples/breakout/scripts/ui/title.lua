-- Title screen of Breakout, copied from the Engine Demo main menu (scripts/ui/main_menu.lua) and cut down:
-- Play starts a fresh run at level 1, Settings opens the settings, Quit leaves. The best score is a setting.

local leaving = false

function on_create()
    ui.transition_fade_in(0.5)
    local best_id = scene.find_entity("BestLabel")
    if best_id ~= 0 then
        ui.set_text(best_id, "Best: " .. settings.get("breakout_best", 0))
    end
end

function on_play_clicked()
    if leaving then return end
    leaving = true
    sound.play("sounds/ui/start.wav")
    gamestate.clear()
    gamestate.set("breakout_score", 0)
    gamestate.set("breakout_lives", 3)
    gamestate.set("breakout_level", 1)
    scene.transition_to("scenes/level_1.owl", "fade_out", 0.3)
end

function on_settings_clicked()
    if leaving then return end
    leaving = true
    scene.transition_to("scenes/settings.owl", "fade_out", 0.3)
end

function on_quit_clicked()
    scene.quit()
end
