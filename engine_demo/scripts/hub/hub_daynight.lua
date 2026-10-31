-- Day and night of the hub: the `DayNightCycle` of this entity keeps the hour (a whole day in `dayLength` seconds),
-- this script turns it into 2D light. The `Ambient` `Light2D` of the entity tints the whole village, white by day,
-- orange at dusk and dawn, a moonlit blue at night; the lamps and windows (`NightLight_<n>` point lights) are lit from
-- dusk to dawn. The adaptive music of the hub follows the `mood_key` game state key: `calm` by day, `night` at night,
-- `danger` while the hero is in the cave. The village of the main menu runs the same script (no cave, no music).
-- Demonstrates: entity.get_time_of_day, entity.set_light_color / set_light_intensity / set_light_enabled on Light2D,
-- an AdaptiveMusic state set from Lua through the game state, gamestate keys as probes (`<probe>_hour`, `<probe>_night`).

properties = {
    { name = "lights_prefix", type = "string", default = "NightLight_" },
    { name = "cave", type = "string", default = "Cave" },
    { name = "player", type = "string", default = "Player" },
    { name = "dusk", type = "float", default = 18.5, min = 0, max = 24 },  -- lamps lit from here...
    { name = "dawn", type = "float", default = 6.0, min = 0, max = 24 },   -- ...until here
    { name = "mood_key", type = "string", default = "hub_mood" },  -- empty: no music state
    { name = "probe", type = "string", default = "hub" },
}

-- Colour and intensity of the ambient light along the day: hour, r, g, b, intensity (linear between two keys).
local KEYS = {
    { 0, 0.3, 0.36, 0.68, 0.42 },
    { 5, 0.3, 0.36, 0.68, 0.42 },
    { 6.5, 1.0, 0.72, 0.55, 0.8 },
    { 8, 1, 1, 1, 1 },
    { 17, 1, 1, 1, 1 },
    { 18.5, 1.0, 0.66, 0.45, 0.85 },
    { 20, 0.3, 0.36, 0.68, 0.42 },
    { 24, 0.3, 0.36, 0.68, 0.42 },
}

local lights = {}
local cave_id = 0
local player_id = 0
local lit = nil
local mood = ""

local function ambient_at(hour)
    for k = 1, #KEYS - 1 do
        local a, b = KEYS[k], KEYS[k + 1]
        if hour >= a[1] and hour <= b[1] then
            local t = (b[1] > a[1]) and (hour - a[1]) / (b[1] - a[1]) or 0
            local function mix(i) return a[i] + (b[i] - a[i]) * t end
            return mix(2), mix(3), mix(4), mix(5)
        end
    end
    return 1, 1, 1, 1
end

local function in_cave()
    if cave_id == 0 or player_id == 0 then  -- a scene without them (the main menu)
        return false
    end
    local px, py = transform.get_position(player_id)
    local cx, cy = transform.get_position(cave_id)
    local sx, sy = transform.get_scale(cave_id)
    return math.abs(px - cx) <= sx / 2 and math.abs(py - cy) <= sy / 2
end

function on_create()
    cave_id = scene.find_entity(cave)
    player_id = scene.find_entity(player)
    local n = 1
    while true do
        local id = scene.find_entity(lights_prefix .. n)
        if id == 0 then
            break
        end
        lights[#lights + 1] = id
        n = n + 1
    end
end

function on_update(_)
    local hour = entity.get_time_of_day(entity_id) or 12
    local r, g, b, intensity = ambient_at(hour)
    entity.set_light_color(entity_id, r, g, b)
    entity.set_light_intensity(entity_id, intensity)
    local night = hour >= dusk or hour < dawn
    if night ~= lit then
        lit = night
        for _, id in ipairs(lights) do
            entity.set_light_enabled(id, night)
        end
        gamestate.set(probe .. "_night", night)
    end
    gamestate.set(probe .. "_hour", math.floor(hour * 10) / 10)
    if mood_key == "" then
        return
    end
    local wanted = in_cave() and "danger" or (night and "night" or "calm")
    if wanted ~= mood then
        mood = wanted
        gamestate.set(mood_key, mood)
    end
end
