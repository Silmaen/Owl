-- Life of the isometric town: smoke from the factory chimneys, glints on the water, blinking lights, gulls over the map and
-- a day and night tint, driven from one script.
-- Demonstrates: moving, scaling and hiding many entities from one script (`scene.find_entity` once for each, then
--               `transform.set_*` and `ui.set_visible` every frame), sprites of the isometric layer (smoke that rises and
--               shrinks, glints that swell and fade, lights that blink) next to sprites of the 2D `overlay` layer drawn
--               over it (animated gulls and the night tint, both pinned to the camera), `sound.play` for a bell and the
--               gulls' chirps, the `volume_sfx` setting.
--
-- Entities by tag: `Chimney_<c>` with `Smoke_<c>_<n>` (4 puffs a chimney), `Sparkle_<n>`, `LampGlow_<n>`, `Beacon_<n>`,
-- `Bird_<n>` and `Tint_<n>` (the first night level is `Tint_1`, each one adds some darkness). The gamestate keys
-- `town_life_smoke`, `town_life_sparkles`, `town_life_lights`, `town_life_birds` (what the script drives) and
-- `town_daylight` (0 by day, up to the number of tints at night) mirror it for the scenario test.

local PUFFS = 4
local CHIMNEY_TOP = 2.85    -- height of the top of a chimney above its foot, in world units
local PUFF_LIFE = 5.0       -- seconds a puff takes to rise and vanish
local PUFF_RISE = 3.2       -- world units a puff climbs in its life
local WIND_X, WIND_Y = 1.6, 0.5

-- The day: seconds of daylight, of dusk (the tint steps up), of night and of dawn.
local DAY, DUSK, NIGHT, DAWN = 100.0, 20.0, 40.0, 20.0
local CYCLE = DAY + DUSK + NIGHT + DAWN

local BELL_EVERY = 50.0
local CHIRP_GAP = 7.0

local camera_id = 0
local clock = 0
local smoke, sparkles, lamps, beacons, birds, tints = {}, {}, {}, {}, {}, {}
local shown_level = -1
local bell_wait, chirp_wait = BELL_EVERY * 0.5, 2.0

local function find_all(prefix, keep)
    local found = {}
    local number = 1
    while true do
        local id = scene.find_entity(prefix .. number)
        if id == 0 then
            return found
        end
        found[#found + 1] = keep(id, number)
        number = number + 1
    end
end

local function play(path, volume)
    gamestate.set("town_sounds", gamestate.get("town_sounds", 0) + 1)
    local handle = sound.play(path)
    sound.set_volume(handle, volume * settings.get("volume_sfx", 1.0))
end

function on_create()
    camera_id = scene.find_entity("MainCamera")
    local chimney = 1
    while true do
        local id = scene.find_entity("Chimney_" .. chimney)
        if id == 0 then
            break
        end
        local x, y, z = transform.get_position(id)
        for puff = 1, PUFFS do
            local puff_id = scene.find_entity("Smoke_" .. chimney .. "_" .. puff)
            if puff_id ~= 0 then
                smoke[#smoke + 1] = { id = puff_id, x = x, y = y, z = z + CHIMNEY_TOP,
                                      phase = ((puff - 1) / PUFFS + chimney * 0.137) % 1 }
            end
        end
        chimney = chimney + 1
    end
    sparkles = find_all("Sparkle_", function(id, number)
        local x, y, z = transform.get_position(id)
        return { id = id, x = x, y = y, z = z, phase = number * 1.7, rate = 1.1 + (number % 5) * 0.23 }
    end)
    lamps = find_all("LampGlow_", function(id, number)
        return { id = id, phase = number * 2.3 }
    end)
    beacons = find_all("Beacon_", function(id, number)
        return { id = id, phase = number * 0.37 }
    end)
    birds = find_all("Bird_", function(id, number)
        return { id = id, speed = 2.4 + 0.35 * number, lane = -7.5 + 2.4 * number, start = number * 9.5, last = -99 }
    end)
    tints = find_all("Tint_", function(id)
        ui.set_visible(id, false)
        return { id = id }
    end)
    gamestate.set("town_life_smoke", #smoke)
    gamestate.set("town_life_sparkles", #sparkles)
    gamestate.set("town_life_lights", #lamps + #beacons)
    gamestate.set("town_life_birds", #birds)
    gamestate.set("town_daylight", 0)
end

-- Smoke: each puff swells, climbs, drifts with the wind and shrinks to nothing, then starts again at the chimney.
local function update_smoke()
    for _, puff in ipairs(smoke) do
        local age = (clock / PUFF_LIFE + puff.phase) % 1
        transform.set_position(puff.id, puff.x + age * WIND_X * 2 + math.sin(age * 6 + puff.phase * 9) * 0.12,
            puff.y + age * WIND_Y * 2, puff.z + age * PUFF_RISE)
        local size = (0.12 + 0.8 * (1 - age)) * math.min(1, age * 10)
        transform.set_scale(puff.id, size, size, 1)
    end
end

-- Water: glints that swell and fade, drifting a little along the current.
local function update_water()
    for _, glint in ipairs(sparkles) do
        local wave = math.sin(clock * glint.rate + glint.phase)
        local size = math.max(0.02, 0.5 * wave * wave)
        transform.set_position(glint.id, glint.x + math.sin(clock * 0.3 + glint.phase) * 0.25, glint.y, glint.z)
        transform.set_scale(glint.id, size * 1.6, size * 0.7, 1)
    end
end

-- Lights: street lamps glow steadily and stronger at night; the red beacons of the towers blink.
local function update_lights(level)
    for _, lamp in ipairs(lamps) do
        local size = 0.3 + 0.07 * level + 0.03 * math.sin(clock * 3 + lamp.phase)
        transform.set_scale(lamp.id, size, size, 1)
    end
    for _, beacon in ipairs(beacons) do
        local on = (clock * 0.7 + beacon.phase) % 1 < 0.4
        local size = on and 0.35 or 0.0
        transform.set_scale(beacon.id, size, size, 1)
    end
end

-- Gulls: they fly across the screen whatever the view, pinned to the camera in the overlay layer.
local function update_birds(camera_x, camera_y, dt)
    chirp_wait = chirp_wait - dt
    for _, bird in ipairs(birds) do
        local across = -24 + (clock * bird.speed + bird.start) % 48
        local height = bird.lane + math.sin(clock * 1.3 + bird.start) * 0.8
        transform.set_position(bird.id, camera_x + across, camera_y + height, 0.5)
        if bird.last < 0 and across >= 0 and chirp_wait <= 0 then
            chirp_wait = CHIRP_GAP
            play("sounds/town/chirp.wav", 0.18)
        end
        bird.last = across
    end
end

-- The day: the tint layers come on one by one at dusk and go off at dawn.
local function daylight_level()
    local t = clock % CYCLE
    local steps = #tints
    if t < DAY then
        return 0
    elseif t < DAY + DUSK then
        return math.floor((t - DAY) / DUSK * steps + 0.999)
    elseif t < DAY + DUSK + NIGHT then
        return steps
    end
    return steps - math.floor((t - DAY - DUSK - NIGHT) / DAWN * steps + 0.999)
end

local function update_tint(camera_x, camera_y)
    local level = daylight_level()
    for index, tint in ipairs(tints) do
        transform.set_position(tint.id, camera_x, camera_y, 0.9)
        if level ~= shown_level then
            ui.set_visible(tint.id, index <= level)
        end
    end
    if level ~= shown_level then
        shown_level = level
        gamestate.set("town_daylight", level)
    end
    return level
end

function on_update(dt)
    clock = clock + dt
    local camera_x, camera_y = transform.get_position(camera_id)
    update_smoke()
    update_water()
    local level = update_tint(camera_x, camera_y)
    update_lights(level)
    update_birds(camera_x, camera_y, dt)
    bell_wait = bell_wait - dt
    if bell_wait <= 0 then
        bell_wait = BELL_EVERY
        play("sounds/town/bell.wav", 0.25)
    end
end
