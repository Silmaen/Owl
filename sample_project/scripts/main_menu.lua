-- Main Menu controller
-- Demonstrates: UI button callbacks, scene loading

function on_create()
    log.info("Main Menu loaded")
end

function on_play_clicked()
    log.info("Play button clicked! Loading gameplay scene...")
    scene.load_scene("scenes/gameplay.owl")
end
