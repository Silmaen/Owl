-- Victory zone of a platformer level — marks the matching house door as
-- explored and returns to the world map. When all houses are explored, the
-- world map's teleporter becomes visible (handled by world_player.lua).

local pending_scene = nil

function on_update(dt)
    if pending_scene and not ui.is_transition_active() then
        scene.load_scene(pending_scene)
        pending_scene = nil
    end
end

-- Bound by Trigger.Type=LuaCallback with CallbackName=on_level_complete.
function on_level_complete(player_id)
    local door = gamestate.get("active_door", 1)
    local key = "house_" .. door .. "_done"
    if gamestate.get(key, 0) > 0 then
        return  -- already counted, avoid double-fire
    end
    gamestate.set(key, 1)
    local visited = gamestate.get("houses_visited", 0) + 1
    gamestate.set("houses_visited", visited)
    log.info("Level complete! Houses visited: " .. visited)
    sound.play("sounds/victory.wav")
    pending_scene = "scenes/world_map.owl"
    ui.transition_fade_out(0.4)
end
