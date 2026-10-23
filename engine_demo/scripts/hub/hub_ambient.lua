-- Life of the hub: smoke rising from the chimneys, cloud shadows drifting over the village and the bell of the church.
-- Demonstrates: moving and scaling many entities from one script (`scene.find_entity` once, `transform.set_*` every
--               frame), CircleRenderer shapes (translucent for the shadows; the smoke shrinks as it
--               rises, the clouds wrap round the map), a one-shot sound on a timer (`sound.play`, the bell rings every
--               a random gap, `bell_rings` in gamestate counts it), random variants and gaps for the birds and the gusts
--               of wind (`math.random`, `sound.set_volume`): the wind bed of the scene is a long loop, the rest is
--               scattered over it so that the ambience never repeats the same way.
--
-- Entities named `Smoke_<house>_<n>` (3 puffs per chimney, the first one's position is the chimney) and `Cloud_<n>`.

properties = {
    { name = "puffs_per_chimney", type = "int", default = 3, min = 1 },
    { name = "puff_life", type = "float", default = 3.0, min = 0.1 },  -- seconds
    { name = "rise", type = "float", default = 1.6 },  -- cells a puff climbs in its life
    { name = "map_left", type = "float", default = -14.0 },
    { name = "map_right", type = "float", default = 78.0 },
    { name = "cloud_speeds", type = "numbers", default = { 0.35, 0.25, 0.45, 0.3, 0.4, 0.28 } },  -- one per `Cloud_<n>`
    { name = "bell_gap", type = "vec2", default = { 100, 170 } },  -- seconds between two rings (min, max)
    { name = "bell_first", type = "float", default = 12.0 },  -- seconds before the first ring
    { name = "bird_gap", type = "vec2", default = { 7, 20 } },
    { name = "bird_volume", type = "vec2", default = { 0.12, 0.3 } },
    { name = "wind_gap", type = "vec2", default = { 18, 40 } },
    { name = "wind_volume", type = "vec2", default = { 0.15, 0.35 } },
}

local HOUSES = { { 9.5, -13.2 }, { 20.5, -13.2 }, { 44.5, -13.2 }, { 54.5, -13.2 }, { 39.5, -12.2 } }
local BIRD_VARIANTS = 4
local WIND_VARIANTS = 3

local puffs = {}
local clouds = {}
local clock = 0
local next_bell = 0
local rings = 0
local next_bird = 3
local next_wind = 9

-- A random number of seconds of a { min, max } range.
local function between(range)
    return range.x + math.random() * (range.y - range.x)
end

-- The variants of each family, spelt out so the game export finds and packs every file.
local FAMILIES = {
    birds = { "sounds/hub/birds_1.wav", "sounds/hub/birds_2.wav", "sounds/hub/birds_3.wav", "sounds/hub/birds_4.wav" },
    wind = { "sounds/hub/wind_1.wav", "sounds/hub/wind_2.wav", "sounds/hub/wind_3.wav" },
}

-- One sound of a family at a random variant and volume.
local function scatter(name, variants, volume)
    local files = FAMILIES[name]
    sound.set_volume(sound.play(files[math.random(math.min(variants, #files))]), between(volume))
end

function on_create()
    next_bell = bell_first
    for h, house in ipairs(HOUSES) do
        for n = 1, puffs_per_chimney do
            local id = scene.find_entity("Smoke_" .. h .. "_" .. n)
            if id ~= 0 then
                puffs[#puffs + 1] = { id = id, x = house[1], y = house[2], phase = (n - 1) / puffs_per_chimney + h * 0.17 }
            end
        end
    end
    for n, speed in ipairs(cloud_speeds) do
        local id = scene.find_entity("Cloud_" .. n)
        if id ~= 0 then
            local x, y, z = transform.get_position(id)
            clouds[#clouds + 1] = { id = id, x = x, y = y, z = z, speed = speed }
        end
    end
end

function on_update(dt)
    clock = clock + dt
    if clock >= next_bell then
        next_bell = clock + between(bell_gap)
        rings = rings + 1
        gamestate.set("bell_rings", rings)
        sound.set_volume(sound.play("sounds/hub/bell.wav"), 0.4)
    end
    if clock >= next_bird then
        next_bird = clock + between(bird_gap)
        scatter("birds", BIRD_VARIANTS, bird_volume)
        gamestate.set("bird_calls", gamestate.get("bird_calls", 0) + 1)
    end
    if clock >= next_wind then
        next_wind = clock + between(wind_gap)
        scatter("wind", WIND_VARIANTS, wind_volume)
    end
    for _, puff in ipairs(puffs) do
        local age = (clock / puff_life + puff.phase) % 1
        transform.set_position(puff.id, puff.x + math.sin(age * 5 + puff.phase * 9) * 0.12, puff.y + age * rise, 0.8)
        local size = 0.55 * (1 - age) + 0.1
        transform.set_scale(puff.id, size, size, 1)
    end
    for _, cloud in ipairs(clouds) do
        cloud.x = cloud.x + cloud.speed * dt
        if cloud.x > map_right then
            cloud.x = map_left
        end
        transform.set_position(cloud.id, cloud.x, cloud.y, cloud.z)
    end
end
