-- Main Menu controller
-- Demonstrates: UI button callbacks, scene.load_scene, gamestate, save.has_save

function on_create()
    log.info("Main Menu loaded")
    -- Check if there's a previous save
    if save.has_save(1) then
        log.info("Save found in slot 1! Previous score: " .. gamestate.get("score", 0))
    else
        log.info("No save found. Starting fresh.")
        gamestate.set("score", 0)
    end
end

function on_play_clicked()
    log.info("Play button clicked! Loading gameplay scene...")
    scene.load_scene("scenes/gameplay.owl")
end

function on_settings_clicked()
    log.info("Settings button clicked! Loading settings menu...")
    scene.load_scene("scenes/settings_menu.owl")
end
