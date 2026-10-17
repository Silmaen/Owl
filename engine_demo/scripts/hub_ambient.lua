-- Life of the hub: smoke rising from the chimneys and cloud shadows drifting over the village.
-- Demonstrates: moving and scaling many entities from one script (`scene.find_entity` once, `transform.set_*` every
--               frame), CircleRenderer shapes (translucent for the shadows; the smoke shrinks as it
--               rises, the clouds wrap round the map).
--
-- Entities named `Smoke_<house>_<n>` (3 puffs per chimney, the first one's position is the chimney) and `Cloud_<n>`.

local HOUSES = { { 8.5, -11.2 }, { 14.5, -11.2 }, { 21.5, -11.2 }, { 32.5, -13.2 } }
local PUFFS = 3
local RISE = 1.6        -- cells a puff climbs in its life
local LIFE = 3.0        -- seconds
local CLOUDS = { { speed = 0.35 }, { speed = 0.25 }, { speed = 0.45 }, { speed = 0.3 } }
local MAP_LEFT, MAP_RIGHT = -12, 44

local puffs = {}
local clouds = {}
local clock = 0

function on_create()
    for h, house in ipairs(HOUSES) do
        for n = 1, PUFFS do
            local id = scene.find_entity("Smoke_" .. h .. "_" .. n)
            if id ~= 0 then
                puffs[#puffs + 1] = { id = id, x = house[1], y = house[2], phase = (n - 1) / PUFFS + h * 0.17 }
            end
        end
    end
    for n, cloud in ipairs(CLOUDS) do
        local id = scene.find_entity("Cloud_" .. n)
        if id ~= 0 then
            local x, y, z = transform.get_position(id)
            clouds[#clouds + 1] = { id = id, x = x, y = y, z = z, speed = cloud.speed }
        end
    end
end

function on_update(dt)
    clock = clock + dt
    for _, puff in ipairs(puffs) do
        local age = (clock / LIFE + puff.phase) % 1
        transform.set_position(puff.id, puff.x + math.sin(age * 5 + puff.phase * 9) * 0.12, puff.y + age * RISE, 0.8)
        local size = 0.55 * (1 - age) + 0.1
        transform.set_scale(puff.id, size, size, 1)
    end
    for _, cloud in ipairs(clouds) do
        cloud.x = cloud.x + cloud.speed * dt
        if cloud.x > MAP_RIGHT then
            cloud.x = MAP_LEFT
        end
        transform.set_position(cloud.id, cloud.x, cloud.y, cloud.z)
    end
end
