-- Victory zone of the platformer level (the door at the end of the tower): marks the level done and returns to the
-- hub behind a fade. The world map counts the levels done (`level_<name>_done`) and wakes its victory portal when all are.

-- Bound by Trigger.Type=LuaCallback with CallbackName=on_level_complete.
function on_level_complete(player_id)
    local door = gamestate.get("active_door", 1)
    local key = "house_" .. door .. "_done"
    if gamestate.get(key, 0) > 0 then
        return  -- already counted, avoid double-fire
    end
    gamestate.set(key, 1)
    gamestate.set("level_platformer_done", 1)
    gamestate.set("platformer_banner", "Level complete!")
    log.info("Platformer level complete!")
    sound.play("sounds/victory.wav")
    scene.transition_to("scenes/world_map.owl", "fade_out", 0.4)
end
