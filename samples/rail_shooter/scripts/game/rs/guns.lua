-- The ship's lasers: a bolt every `fire_interval` while Fire is held, from the ship to the far reticle. The nearest
-- target close to the line of the reticle (within `aim_assist`, an angle in radians) takes the bolt straight at it.
-- A bolt flies in rail coordinates and is tested as a segment each frame against the enemies and the rocks ahead.

local rs = require("scripts.game.rs.core")
local field = require("scripts.game.rs.field")
local foes = require("scripts.game.rs.foes")
local S, C = rs.state, rs.cfg

local G = {}

-- The nearest target close to the line from the ship through the reticle, and its rail position.
local function assisted(ax, ay)
    local nearest, bs, bx, by = math.huge, nil, nil, nil
    local reach = C.reticle_depth
    local function consider(s, x, y, radius)
        local along = s - S.s
        if along < 6 or along >= nearest then return end
        local lx = S.ship_x + (ax - S.ship_x) * along / reach
        local ly = S.ship_y + (ay - S.ship_y) * along / reach
        local off = math.max(0, math.sqrt((x - lx) ^ 2 + (y - ly) ^ 2) - radius) / along
        if off < C.aim_assist then nearest, bs, bx, by = along, s, x, y end
    end
    for _, foe in ipairs(S.enemies) do
        if foe.alive then consider(S.s + foe.dz, foe.x, foe.y, foe.radius) end
    end
    field.each_near(160, function(rock) consider(rock.s, rock.x, rock.y, rock.radius) return true end)
    return bs, bx, by
end

function G.fire()
    local ts, tx, ty = assisted(S.aim_x, S.aim_y)
    local ds, dx, dy
    if ts ~= nil then
        ds, dx, dy = ts - S.s, tx - S.ship_x, ty - S.ship_y
    else
        ds, dx, dy = C.reticle_depth, S.aim_x - S.ship_x, S.aim_y - S.ship_y
    end
    local n = math.sqrt(ds * ds + dx * dx + dy * dy)
    local speed = C.bolt_speed
    local s, x, y = S.s - 1.0, S.ship_x, S.ship_y
    local id = scene.spawn_prefab("prefabs/bolt.owlprefab", rs.to_scene(s, x, y))
    if id == 0 then return end
    S.bolts[#S.bolts + 1] = {
        id = id, s = s, x = x, y = y,
        vs = ds / n * speed + S.speed, vx = dx / n * speed, vy = dy / n * speed, life = 1.6,
    }
    S.shots = S.shots + 1
    rs.play("sounds/game/laser.wav")
end

-- Where a bolt coming from (s0, x0, y0) meets the surface of a target of a radius around (ts, tx, ty).
local function impact(s0, x0, y0, ts, tx, ty, radius)
    local ds, dx, dy = s0 - ts, x0 - tx, y0 - ty
    local n = math.sqrt(ds * ds + dx * dx + dy * dy)
    if n < 1e-6 then return ts, tx, ty end
    local k = radius * 0.8 / n
    return ts + ds * k, tx + dx * k, ty + dy * k
end

local function strike(bolt, s0, x0, y0)
    local vs, vx, vy = bolt.vs - S.speed, bolt.vx, bolt.vy
    for _, foe in ipairs(S.enemies) do
        if foe.alive then
            local fs = S.s + foe.dz
            if rs.segment_distance(s0, x0, y0, bolt.s, bolt.x, bolt.y, fs, foe.x, foe.y) < foe.radius then
                local is, ix, iy = impact(s0, x0, y0, fs, foe.x, foe.y, foe.radius)
                foes.hit(foe, is, ix, iy, vs, vx, vy)
                return true
            end
        end
    end
    local struck = false
    field.each_near(260, function(rock)
        if rock.s + rock.radius < s0 then return true end
        if rock.s - rock.radius > bolt.s then return false end
        if rs.segment_distance(s0, x0, y0, bolt.s, bolt.x, bolt.y, rock.s, rock.x, rock.y) < rock.radius then
            local is, ix, iy = impact(s0, x0, y0, rock.s, rock.x, rock.y, rock.radius)
            field.hit(rock, is, ix, iy, vs, vx, vy)
            struck = true
            return false
        end
        return true
    end)
    return struck
end

function G.update(dt)
    local kept = {}
    for _, bolt in ipairs(S.bolts) do
        local s0, x0, y0 = bolt.s, bolt.x, bolt.y
        bolt.s, bolt.x, bolt.y = s0 + bolt.vs * dt, x0 + bolt.vx * dt, y0 + bolt.vy * dt
        bolt.life = bolt.life - dt
        local alive = bolt.life > 0
        if alive and strike(bolt, s0, x0, y0) then
            S.hits = S.hits + 1
            alive = false
        end
        if alive then
            rs.place(bolt.id, bolt.s, bolt.x, bolt.y, bolt.vs - S.speed, bolt.vx, bolt.vy)
            kept[#kept + 1] = bolt
        else
            scene.destroy_entity(bolt.id)
        end
    end
    S.bolts = kept
end

function G.clear()
    for _, bolt in ipairs(S.bolts) do scene.destroy_entity(bolt.id) end
    S.bolts = {}
end

return G
