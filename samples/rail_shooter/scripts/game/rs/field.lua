-- The asteroid field: the `Asteroid` entities of the scene, read once in rail coordinates and sorted by distance.
-- Only the rocks around the carrier are looked at each frame (from `rock_first`). A small rock breaks at the first
-- hit, a big one (`big_rock` across and more) takes three; a rock the ship flies into breaks and costs hull. A hit
-- flashes the rock and knocks it a little along the shot (for the eye only: its rail position does not move).

local rs = require("scripts.game.rs.core")
local fx = require("scripts.game.rs.fx")
local S, C = rs.state, rs.cfg

local F = {}

function F.load()
    S.asteroids = {}
    for _, id in ipairs(scene.find_entities("Asteroid")) do
        local wx, wy, wz = transform.get_position(id)
        local s, x, y = rs.to_rail(wx, wy, wz)
        local scale = transform.get_scale(id)
        local radius = scale * 0.95
        local rx, ry, rz = transform.get_rotation(id)
        S.asteroids[#S.asteroids + 1] = {
            id = id, s = s, x = x, y = y, radius = radius, hp = radius >= C.big_rock and 3 or 1,
            big = radius >= C.big_rock, alive = true, rx = rx, ry = ry, rz = rz,
            spin = 0.2 + (id % 7) * 0.08, knock = 0.0, kick = { 0, 0, 0 },
        }
    end
    table.sort(S.asteroids, function(a, b) return a.s < b.s end)
    S.rock_first = 1
end

-- The rocks from just behind the ship to `range` ahead, through a callback; false from it stops the walk.
function F.each_near(range, callback)
    local list = S.asteroids
    while S.rock_first <= #list and list[S.rock_first].s < S.s - 12 do
        S.rock_first = S.rock_first + 1
    end
    for i = S.rock_first, #list do
        local rock = list[i]
        if rock.s > S.s + range then break end
        if rock.alive and callback(rock) == false then return end
    end
end

function F.break_rock(rock)
    rock.alive = false
    scene.destroy_entity(rock.id)
    fx.explode(rock.s, rock.x, rock.y, rock.radius, nil, nil, nil, true)
    rs.play("sounds/game/rock_break.wav")
end

-- A bolt hit the rock at a point (rail coordinates), flying along (ds, dx, dy): true when it broke.
function F.hit(rock, is, ix, iy, ds, dx, dy)
    rock.hp = rock.hp - 1
    if rock.hp > 0 then
        rs.burst("sparks", is or rock.s, ix or rock.x, iy or rock.y, 14)
        rs.burst("debris", is or rock.s, ix or rock.x, iy or rock.y, 10)
        rs.play("sounds/game/rock_hit.wav")
        fx.flash(rock.id, 2.4, 2.1, 1.9)
        -- The scene direction of the shot, to knock the rock along it.
        local ax, ay, az = rs.to_scene(rock.s, 0, 0)
        local bx, by, bz = rs.to_scene(rock.s + (ds or 1), dx or 0, dy or 0)
        local n = math.sqrt((bx - ax) ^ 2 + (by - ay) ^ 2 + (bz - az) ^ 2)
        if n > 1e-6 then rock.kick = { (bx - ax) / n, (by - ay) / n, (bz - az) / n } end
        if rock.knock <= 0 then rock.home = { transform.get_position(rock.id) } end
        rock.knock = 0.35
        return false
    end
    F.break_rock(rock)
    S.rocks = S.rocks + 1
    rs.add_score(rock.big and C.big_rock_points or C.rock_points)
    return true
end

function F.update(dt)
    F.each_near(220, function(rock)
        -- The near rocks turn slowly on themselves.
        if rock.s < S.s + 120 then
            rock.ry = rock.ry + rock.spin * dt
            local wobble = rock.knock * 0.8
            transform.set_rotation(rock.id, rock.rx + wobble, rock.ry, rock.rz - wobble)
        end
        -- A knocked rock is pushed back along the shot, then settles back home.
        if rock.knock > 0 then
            rock.knock = math.max(0, rock.knock - dt)
            local push = math.sin(rock.knock / 0.35 * math.pi) * 0.5 * rock.radius
            local h, k = rock.home, rock.kick
            transform.set_position(rock.id, h[1] + k[1] * push, h[2] + k[2] * push, h[3] + k[3] * push)
        end
        -- The ship flying into a rock.
        local ds, dx, dy = rock.s - S.s, rock.x - S.ship_x, rock.y - S.ship_y
        local reach = rock.radius + C.ship_radius
        if math.abs(ds) < reach and dx * dx + dy * dy + ds * ds < reach * reach then
            F.break_rock(rock)
            rs.hurt(C.rock_damage, "rock")
        end
        return true
    end)
end

return F
