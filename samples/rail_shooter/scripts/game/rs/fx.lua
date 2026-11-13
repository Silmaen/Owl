-- The effects of Ring Runner: the flash and the knock of a hit, the explosions, the camera shake. Cosmetic only: no
-- rule of the run reads them, and their pseudo-random sequence is their own (a run plays the same each time).
--
-- - Every piece is a pooled entity of the level, taken in turn: the particle emitters (`Flare`, `Burst` the fireball,
--   `Smoke`, `Ember`, `Debris`, `Spark`), the point lights (`Flash`), the shock wave rings (`Shockwave`), the shards of hull
--   (`Shard`) and the chunks of rock (`RockChunk`). A piece taken while still flying starts again.
-- - An explosion of a given `size` (a fighter is 1) bursts a hot core, a fireball, smoke and embers, lights a flash,
--   throws shards or chunks tumbling out, sends a shock wave ring and shakes the camera; a big one chains a few
--   smaller pops around it. Rocks break into tumbling chunks.
-- - The camera shake grows with the size and fades with the distance; the `reduced_motion` setting turns it off.

local rs = require("scripts.game.rs.core")
local S = rs.state

local X = {}

local SHARDS, CHUNKS, FLASHES, RINGS = 18, 18, 3, 3
local pools = { shard = {}, chunk = {}, flash = {}, ring = {} }
local cursor = { shard = 1, chunk = 1, flash = 1, ring = 1 }
local pieces, lights, rings, pops, tints = {}, {}, {}, {}, {}
local trauma, still = 0.0, false
local seed = 20261101

local function rand()
    seed = (seed * 20077 + seed * 16838 % 32768 * 65536 + 12345) % 2147483648
    return seed / 2147483648
end

local function between(low, high)
    return low + (high - low) * rand()
end

-- A direction of the unit sphere, in rail coordinates (along, across, up).
local function direction()
    local z = between(-1, 1)
    local a = between(0, 2 * math.pi)
    local r = math.sqrt(1 - z * z)
    return z, r * math.cos(a), r * math.sin(a)
end

local function take(kind)
    local pool = pools[kind]
    if #pool == 0 then return 0 end
    local id = pool[cursor[kind]]
    cursor[kind] = cursor[kind] % #pool + 1
    return id
end

function X.load()
    local function fill(kind, tag, count)
        for index = 1, count do
            local id = scene.find_entity(tag .. index)
            if id ~= 0 then table.insert(pools[kind], id) end
        end
    end
    fill("shard", "Shard", SHARDS)
    fill("chunk", "RockChunk", CHUNKS)
    fill("flash", "Flash", FLASHES)
    fill("ring", "Shockwave", RINGS)
    for index = 1, 4 do
        S.flares[index] = scene.find_entity("Flare" .. index)
        S.smokes[index] = scene.find_entity("Smoke" .. index)
        S.embers[index] = scene.find_entity("Ember" .. index)
    end
    for _, id in ipairs(pools.flash) do entity.set_light_enabled(id, false) end
end

-- A hit flash on a mesh: its tint jumps to a bright colour and comes back to the one it had.
function X.flash(id, r, g, b)
    if id == nil or id == 0 then return end
    local tint = tints[id]
    if tint == nil then
        local br, bg, bb, ba = entity.get_tint(id)
        if br == nil then return end
        tint = { base = { br, bg, bb, ba } }
        tints[id] = tint
    end
    tint.colour, tint.t, tint.life = { r, g, b }, 0.0, 0.12
    entity.set_tint(id, r, g, b, tint.base[4])
end

local function update_tints(dt)
    for id, tint in pairs(tints) do
        tint.t = tint.t + dt
        local k = math.max(0, 1 - tint.t / tint.life)
        local base, c = tint.base, tint.colour
        local done = not entity.set_tint(id, base[1] + (c[1] - base[1]) * k, base[2] + (c[2] - base[2]) * k,
                base[3] + (c[3] - base[3]) * k, base[4])
        if done or k <= 0 then tints[id] = nil end
    end
end

-- Shake the camera by an amount (an explosion of size 1 next to the ship is about 0.3).
function X.shake(amount)
    trauma = math.min(1, trauma + amount)
end

--- The offset of the camera this frame (zero with reduced motion).
---@return any result
---@return any result_2
function X.camera_offset()
    if still or trauma <= 0 then return 0, 0 end
    local k = trauma * trauma * 0.7
    local t = S.clock
    return k * (math.sin(t * 47) + 0.5 * math.sin(t * 89 + 1.3)), k * (math.sin(t * 53 + 2.1) + 0.5 * math.sin(t * 97))
end

--- Whether the camera may shake (the `reduced_motion` accessibility setting is off).
---@return any result
function X.may_shake()
    return not still
end

local function light(s, x, y, peak, life)
    local id = take("flash")
    if id == 0 then return end
    local wx, wy, wz = rs.to_scene(s, x, y)
    transform.set_position(id, wx, wy, wz)
    entity.set_light_intensity(id, peak)
    entity.set_light_enabled(id, true)
    lights[id] = { t = 0.0, life = life, peak = peak }
end

local function ring(s, x, y, radius)
    local id = take("ring")
    if id == 0 then return end
    rings[id] = { s = s, x = x, y = y, t = 0.0, life = 0.55, radius = radius }
    ui.set_visible(id, true)
end

-- Throw `count` pieces of a pool out of a point: shards of hull tinted by the ship, or chunks of rock.
local function throw(kind, count, s, x, y, scale, speed, r, g, b)
    for _ = 1, count do
        local id = take(kind)
        if id == 0 then return end
        local ds, dx, dy = direction()
        local v = between(speed * 0.5, speed)
        local size = between(scale * 0.6, scale)
        pieces[id] = {
            s = s + ds * scale, x = x + dx * scale, y = y + dy * scale,
            vs = ds * v, vx = dx * v, vy = dy * v,
            rx = between(0, 6.3), ry = between(0, 6.3), rz = between(0, 6.3),
            wx = between(-9, 9), wy = between(-9, 9), wz = between(-9, 9),
            t = 0.0, life = between(1.0, 1.8), size = size,
        }
        if r ~= nil then entity.set_tint(id, r, g, b) end
        transform.set_scale(id, size, size, size)
        ui.set_visible(id, true)
    end
end

-- A small explosion: the pops chained around a big one.
local function pop(s, x, y, size)
    rs.burst("flares", s, x, y, 5)
    rs.burst("bursts", s, x, y, math.floor(12 * size))
    rs.burst("embers", s, x, y, math.floor(12 * size))
    light(s, x, y, 40000 * size, 0.2)
    rs.play("sounds/game/pop.wav")
end

--- An explosion of a ship (`r`, `g`, `b`: the tint of its shards) or of a rock (`rock` true) at rail coordinates.
---@param s any
---@param x any
---@param y any
---@param size any
---@param r any
---@param g any
---@param b any
---@param rock any
function X.explode(s, x, y, size, r, g, b, rock)
    local cap = math.min(size, 3)
    rs.burst("flares", s, x, y, math.floor(4 + 3 * cap))
    rs.burst("smokes", s, x, y, math.floor(16 * cap))
    rs.burst("embers", s, x, y, math.floor(18 * cap))
    if rock then
        rs.burst("debris", s, x, y, math.floor(40 * cap))
        rs.burst("bursts", s, x, y, math.floor(8 * cap))
        throw("chunk", size >= 1 and 6 or 4, s, x, y, 0.5 * size, 10 + 3 * cap)
        light(s, x, y, 30000 * cap, 0.25)
    else
        rs.burst("bursts", s, x, y, math.floor(30 * cap))
        throw("shard", math.floor(5 + 2 * cap), s, x, y, 0.9 * math.sqrt(size), 16 + 4 * cap, r, g, b)
        light(s, x, y, 50000 * cap, 0.4)
        ring(s, x, y, 9 * math.sqrt(size))
    end
    -- Big targets chain a few pops around them, one after the other.
    local chain = size >= 3 and 7 or (size >= 1.4 and 2 or 0)
    local spread = size * 2
    for k = 1, chain do
        local ds, dx, dy = direction()
        pops[#pops + 1] = {
            t = k * (size >= 3 and 0.2 or 0.14) + between(0, 0.08),
            s = s + ds * spread, x = x + dx * spread, y = y + dy * spread * 0.6,
            size = size >= 3 and between(1.2, 1.8) or between(0.6, 0.9),
        }
    end
    X.shake(0.32 * size / (1 + math.abs(s - S.s) / 35))
end

function X.update(dt)
    still = settings.get("reduced_motion", false) == true
    trauma = math.max(0, trauma - dt * 1.6)
    update_tints(dt)
    for id, l in pairs(lights) do
        l.t = l.t + dt
        local k = 1 - l.t / l.life
        if k <= 0 then
            entity.set_light_enabled(id, false)
            lights[id] = nil
        else
            entity.set_light_intensity(id, l.peak * k * k)
        end
    end
    for id, w in pairs(rings) do
        w.t = w.t + dt
        local k = w.t / w.life
        if k >= 1 then
            ui.set_visible(id, false)
            rings[id] = nil
        else
            local radius = w.radius * (0.15 + 0.85 * math.sqrt(k))
            rs.place(id, w.s, w.x, w.y, 1, 0, 0)
            transform.set_scale(id, radius, radius, radius)
            entity.set_tint(id, 1, 1, 1, 0.6 * (1 - k) * (1 - k))
        end
    end
    for id, p in pairs(pieces) do
        p.t = p.t + dt
        if p.t >= p.life then
            ui.set_visible(id, false)
            pieces[id] = nil
        else
            local drag = math.max(0, 1 - dt * 0.8)
            p.vs, p.vx, p.vy = p.vs * drag, p.vx * drag, p.vy * drag
            p.s, p.x, p.y = p.s + p.vs * dt, p.x + p.vx * dt, p.y + p.vy * dt
            p.rx, p.ry, p.rz = p.rx + p.wx * dt, p.ry + p.wy * dt, p.rz + p.wz * dt
            local wx, wy, wz = rs.to_scene(p.s, p.x, p.y)
            transform.set_position(id, wx, wy, wz)
            transform.set_rotation(id, p.rx, p.ry, p.rz)
            local fade = math.min(1, (p.life - p.t) / (p.life * 0.3))
            local size = p.size * fade
            transform.set_scale(id, size, size, size)
        end
    end
    local waiting = {}
    for _, p in ipairs(pops) do
        p.t = p.t - dt
        if p.t <= 0 then pop(p.s, p.x, p.y, p.size) else waiting[#waiting + 1] = p end
    end
    pops = waiting
end

return X
