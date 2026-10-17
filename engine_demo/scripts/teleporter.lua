-- Victory portal - appears when every level of the hub is done (world_player.lua), leads to victory.
-- Demonstrates: a sound retriggered on a timer and mixed by distance (`sound.play`, `sound.set_volume`: the hum of the awake
--               portal fades with the distance to the hero, silent beyond HUM_RANGE cells; `portal_hum` in gamestate counts the hums).

local HUM_PERIOD = 2.2  -- the hum clip lasts a little more, so each one fades into the next
local HUM_RANGE = 16
local HUM_VOLUME = 0.9

local clock = 0
local hums = 0

function on_create()
    log.trace("Teleporter standing by")
    gamestate.set("portal_hum", 0)
end

function on_update(dt)
    if gamestate.get("levels_done", 0) < 4 then
        return  -- asleep: no hum
    end
    clock = clock - dt
    if clock > 0 then
        return
    end
    clock = HUM_PERIOD
    local player = scene.find_entity("Player")
    if player == 0 then
        return
    end
    local px, py = transform.get_position(player)
    local x, y = transform.get_position(entity_id)
    local near = 1 - math.sqrt((px - x) ^ 2 + (py - y) ^ 2) / HUM_RANGE
    if near > 0 then
        sound.set_volume(sound.play("sounds/hub_portal_hum.wav"), near * near * HUM_VOLUME)
        hums = hums + 1
        gamestate.set("portal_hum", hums)
    end
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_teleporter_enter.
function on_teleporter_enter(player_id)
    if gamestate.get("levels_done", 0) < 4 then
        return  -- the portal is asleep until every level is done
    end
    log.info("Teleporter activated! Loading victory scene.")
    sound.play("sounds/hub_portal_wake.wav")
    -- Clear continue state — game is finished.
    gamestate.set("has_continue", false)
    gamestate.remove("continue_scene")
    scene.transition_to("scenes/victory.owl", "fade_out", 0.3)
end
