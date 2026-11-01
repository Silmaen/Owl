-- Victory zone of the platformer level (the door at the end of the tower): marks the level done and returns to the
-- hub behind a fade. The world map counts the levels done (`level_<name>_done`) and wakes its victory portal when all are.

local completed = false

-- Bound by Trigger.Type=LuaCallback with CallbackName=on_level_complete.
function on_level_complete(_)
    if completed then
        return  -- the zone fires while the player stands in it: count it once
    end
    completed = true
    gamestate.set("level_platformer_done", 1)
    gamestate.set("platformer_banner", "Level complete!")
    log.info("Platformer level complete!")
    sound.play("sounds/ui/victory.wav")
    scene.transition_to("scenes/world_map.owl", "fade_out", 0.4)
end
