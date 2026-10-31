-- The asteroid field: the `Asteroid` entities of the scene, read once in rail coordinates and sorted by distance.
-- Only the rocks around the carrier are looked at each frame (from `rock_first`). A small rock breaks at the first
-- hit, a big one (`big_rock` across and more) takes three; a rock the ship flies into breaks and costs hull.

local rs = require("scripts.game.rs.core")
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
            spin = 0.2 + (id % 7) * 0.08,
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
    rs.burst("debris", rock.s, rock.x, rock.y, rock.big and 70 or 30)
    rs.play("sounds/game/rock_break.wav")
end

-- A bolt hit the rock: true when it broke.
function F.hit(rock)
    rock.hp = rock.hp - 1
    if rock.hp > 0 then
        rs.burst("sparks", rock.s, rock.x, rock.y, 14)
        rs.play("sounds/game/hit.wav")
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
            transform.set_rotation(rock.id, rock.rx, rock.ry, rock.rz)
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
