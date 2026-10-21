-- Tavern: a table by the fireplace. The fire light flickers (layered noise on its intensity, a warm colour shift and a
-- small jitter of its position) and the flame cards breathe with it; every candle light wavers on its own seed and
-- frequencies, with quick dips, and the table candle's flame sways. Escape (or Start) returns to the main menu, the
-- FlyCamera component moves the view.
-- Demonstrates: scene.find_entity, entity.set_light_intensity / set_light_color, transform.set_position / set_scale.

local FIRE_LUMENS = 1800
local FIRE_COLOR = { 1.0, 0.55, 0.22 }
local CANDLE_LUMENS = 12
local MANTEL_CANDLES = 11

local leaving = false
local time = 0.0
local fire, flames
local fire_x, fire_y, fire_z = 0, 0, 0
local flame_sx, flame_sy, flame_sz = 1, 1, 1
local candles = {}
local table_flame
local table_flame_x, table_flame_y, table_flame_z = 0, 0, 0

-- Smooth 1D value noise in [-1, 1]: a hashed value per integer, eased in between.
local function hash(n)
    local x = math.sin(n * 127.1 + 311.7) * 43758.5453
    return (x - math.floor(x)) * 2 - 1
end

local function noise(t)
    local i = math.floor(t)
    local f = t - i
    local u = f * f * (3 - 2 * f)
    return hash(i) * (1 - u) + hash(i + 1) * u
end

-- Fire flicker in about [-1, 1]: slow swell, quick licks and a fast crackle.
local function flicker(t, seed)
    return 0.5 * noise(t * 1.3 + seed) + 0.3 * noise(t * 4.7 + seed * 2.1) + 0.2 * noise(t * 11.0 + seed * 3.7)
        + 0.08 * math.sin(t * 17.0 + seed)
end

-- A candle with its own seed and frequencies, so no two candles move together.
local function make_candle(id, seed)
    local s, x, y, z = seed, transform.get_position(id)
    return {
        id = id, x = x, y = y, z = z, seed = s,
        slow = 0.6 + 0.5 * (hash(s) * 0.5 + 0.5), fast = 3.0 + 3.0 * (hash(s + 1) * 0.5 + 0.5),
        dip_rate = 0.35 + 0.4 * (hash(s + 2) * 0.5 + 0.5),
    }
end

-- Candle intensity factor: a wavering of about +-35 % and, now and then, a quick dip when a draught bends the flame.
local function candle_factor(c, t)
    local waver = 0.22 * noise(t * c.slow + c.seed) + 0.13 * noise(t * c.fast + c.seed * 1.7)
    local gust = noise(t * c.dip_rate + c.seed * 2.3)
    local dip = gust > 0.55 and (gust - 0.55) / 0.45 * 0.5 or 0.0
    return math.max(0.2, 1.0 + waver - dip)
end

function on_create()
    fire = scene.find_entity("FireLight")
    flames = scene.find_entity("Flames")
    fire_x, fire_y, fire_z = transform.get_position(fire)
    flame_sx, flame_sy, flame_sz = transform.get_scale(flames)
    for i = 1, MANTEL_CANDLES do
        local id = scene.find_entity("CandleLight" .. i)
        if id ~= 0 then
            candles[#candles + 1] = make_candle(id, 37.0 * i + 11.0)
        end
    end
    local table_light = scene.find_entity("CandleLightTable")
    if table_light ~= 0 then
        candles[#candles + 1] = make_candle(table_light, 913.0)
    end
    table_flame = scene.find_entity("CandleFlame")
    table_flame_x, table_flame_y, table_flame_z = transform.get_position(table_flame)
end

function on_update(dt)
    if not leaving and input.is_action_pressed("pause") then
        leaving = true
        scene.transition_to("scenes/main_menu.owl", "fade", 0.3)
        return
    end
    time = time + dt

    local k = flicker(time, 0.0)
    entity.set_light_intensity(fire, FIRE_LUMENS * (1.0 + 0.28 * k))
    -- Brighter moments burn a little yellower, dimmer ones redder.
    entity.set_light_color(fire, FIRE_COLOR[1], FIRE_COLOR[2] + 0.06 * k, FIRE_COLOR[3] + 0.04 * k)
    transform.set_position(fire, fire_x + 0.03 * noise(time * 3.1 + 7.0), fire_y + 0.04 * noise(time * 3.7 + 13.0),
        fire_z + 0.02 * noise(time * 2.9 + 21.0))

    local lick = flicker(time + 0.05, 0.0)
    transform.set_scale(flames, flame_sx * (1.0 + 0.05 * noise(time * 5.0 + 3.0)), flame_sy * (1.0 + 0.16 * lick),
        flame_sz * (1.0 + 0.05 * noise(time * 4.4 + 9.0)))

    local table_factor = 1.0
    for _, c in ipairs(candles) do
        local factor = candle_factor(c, time)
        entity.set_light_intensity(c.id, CANDLE_LUMENS * factor)
        -- The light sways with its flame by a few millimetres.
        transform.set_position(c.id, c.x + 0.004 * noise(time * c.fast + c.seed * 3.1), c.y,
            c.z + 0.004 * noise(time * c.fast + c.seed * 4.3))
        table_factor = factor
    end

    -- The table candle is the last one: its flame cards shrink in a dip and lean with the draught.
    transform.set_scale(table_flame, 0.9 + 0.1 * table_factor, 0.65 + 0.35 * table_factor, 0.9 + 0.1 * table_factor)
    transform.set_position(table_flame, table_flame_x + 0.002 * noise(time * 5.3 + 2.0), table_flame_y,
        table_flame_z + 0.002 * noise(time * 4.1 + 6.0))
end
