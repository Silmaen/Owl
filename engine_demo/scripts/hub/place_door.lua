-- The door of a place of the hub (a house, the telescope of the observatory): E nearby opens its scene. The world
-- position is kept so the hero comes back to the same spot; `on_enter` sets game state keys first ("key=value" pairs
-- separated by ";", a number or a string each) for the scene opened. While the place is not done (`done_key` unset in
-- the game state) its `HudMarker` shows the way and its dot lights on the minimap; once done both go out
-- (`marked_<done_key>` in the game state tells which, for the scenarios).
-- Demonstrates: one script for every door through its properties, `entity.set_hud_marker_enabled`, `ui.set_visible`,
-- `scene.transition_to` with the transition of each place.

properties = {
    { name = "target_scene", type = "string", default = "scenes/platformer_house.owl" },
    { name = "transition", type = "string", default = "fade" },
    { name = "duration", type = "float", default = 0.3, min = 0 },
    { name = "done_key", type = "string", default = "" },
    { name = "on_enter", type = "string", default = "" },
    { name = "minimap_dot", type = "string", default = "" },
    { name = "door_sound", type = "string", default = "sounds/hub/door.wav" },
}

local entering = false

local function is_done()
    return done_key ~= "" and gamestate.get(done_key, 0) > 0
end

function on_create()
    local done = is_done()
    entity.set_hud_marker_enabled(entity_id, not done)
    if done_key ~= "" then
        gamestate.set("marked_" .. done_key, done and 0 or 1)
    end
    if minimap_dot ~= "" then
        local dot_id = scene.find_entity(minimap_dot)
        if dot_id ~= 0 then
            ui.set_visible(dot_id, not done)
        end
    end
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_door_interact.
function on_door_interact(_)
    if entering then
        return
    end
    entering = true
    local player = scene.find_entity("Player")
    if player ~= 0 then
        local px, py = transform.get_position(player)
        gamestate.set("world_return_x", px)
        gamestate.set("world_return_y", py)
    end
    for key, value in string.gmatch(on_enter, "([%w_]+)%s*=%s*([^;]*)") do
        gamestate.set(key, tonumber(value) or value)
    end
    if door_sound ~= "" then
        sound.play(door_sound)
    end
    log.info("Entering " .. target_scene)
    scene.transition_to(target_scene, transition, duration)
end
