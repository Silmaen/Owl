-- Death zone trigger
-- Demonstrates: LuaCallback trigger, scene.load_scene, sound.play, gamestate

function on_create()
end

function on_update(dt)
end

function on_death_zone()
    sound.play("sounds/death.wav")
    scene.load_scene("scenes/game_over.owl")
end
