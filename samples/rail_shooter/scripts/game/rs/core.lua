-- Shared state of a Ring Runner run and the rail coordinates every module works in.
--
-- A position of the game is (s, x, y): `s` the distance along the rail, `x` across it (to the right of the
-- direction of flight) and `y` up from it. The frame at a distance is the tangent of the rail, its right (the tangent
-- crossed with the world up) and its up (the right crossed with the tangent): the axes of the `RailCarrier`, whose
-- `SplineFollower` faces the rail. Placing an entity samples the rail at its distance (`spline.sample`), so the
-- floating origin may shift the scene: the rail coordinates stay the same.

local M = {}

-- The run: one table, filled by `rail.lua` in `on_create`.
M.state = {
    rail = 0, carrier = 0, ship = 0, camera = 0, reticle = 0, reticle_near = 0,
    s = 0.0,                -- distance of the carrier
    ship_x = 0.0, ship_y = 0.0, vx = 0.0, vy = 0.0,
    aim_x = 0.0, aim_y = 0.0, mouse_aim = false, mouse_x = -1, mouse_y = -1,
    hull = 100.0, hull_max = 100.0,
    score = 0, shots = 0, hits = 0, kills = 0, foes = 0, rocks = 0, rolls = 0,
    roll_time = 0.0, roll_dir = 0, roll_cooldown = 0.0,
    fire_clock = 0.0, hurt_clock = 0.0,
    phase = "intro",        -- intro, play, boss_intro, boss, outro, dead
    clock = 0.0, phase_clock = 0.0,
    paused = false,
    bolts = {}, enemy_bolts = {}, enemies = {}, asteroids = {}, rock_first = 1,
    wave_rows = {}, next_wave = 1, boss = nil, boss_distance = 3000.0, boss_dead = false, deflected = 0, speed = 0.0,
    bursts = {}, debris = {}, sparks = {}, bursts_next = 1, debris_next = 1, sparks_next = 1,
    hud = {}, shown = {}, message_clock = 0.0, taken = {},
}

-- Tuning, copied from the properties of the script in `on_create`.
M.cfg = {}

-- The frame of the rail at a distance: its point, tangent, right and up.
function M.frame(s)
    local S = M.state
    local px, py, pz = spline.sample(S.rail, s)
    local tx, ty, tz = spline.get_tangent(S.rail, s)
    if px == nil or tx == nil then return 0, 0, 0, 0, 0, -1, 1, 0, 0, 0, 1, 0 end
    local rx, rz = -tz, tx
    local n = math.sqrt(rx * rx + rz * rz)
    if n < 1e-6 then rx, rz, n = 1, 0, 1 end
    rx, rz = rx / n, rz / n
    local ux, uy, uz = -rz * ty, rz * tx - rx * tz, rx * ty
    return px, py, pz, tx, ty, tz, rx, 0, rz, ux, uy, uz
end

-- Scene position of rail coordinates.
function M.to_scene(s, x, y)
    local px, py, pz, _, _, _, rx, ry, rz, ux, uy, uz = M.frame(s)
    return px + rx * x + ux * y, py + ry * x + uy * y, pz + rz * x + uz * y
end

-- Rail coordinates of a scene position, near a distance found by `spline.get_nearest`.
function M.to_rail(wx, wy, wz)
    local S = M.state
    local s = spline.get_nearest(S.rail, wx, wy, wz) or 0
    local px, py, pz, _, _, _, rx, ry, rz, ux, uy, uz = M.frame(s)
    local dx, dy, dz = wx - px, wy - py, wz - pz
    return s, dx * rx + dy * ry + dz * rz, dx * ux + dy * uy + dz * uz
end

-- Rotation (pitch, yaw) of an entity whose -Z axis looks along a scene direction.
function M.look(dx, dy, dz)
    local n = math.sqrt(dx * dx + dy * dy + dz * dz)
    if n < 1e-6 then return 0, 0 end
    return math.asin(math.max(-1, math.min(1, dy / n))), math.atan(-dx, -dz)
end

-- Place an entity at rail coordinates, its nose along a rail direction (ds, dx, dy), banked by `roll`.
function M.place(id, s, x, y, ds, dx, dy, roll)
    local px, py, pz, tx, ty, tz, rx, ry, rz, ux, uy, uz = M.frame(s)
    transform.set_position(id, px + rx * x + ux * y, py + ry * x + uy * y, pz + rz * x + uz * y)
    if ds ~= nil then
        local wx = tx * ds + rx * dx + ux * dy
        local wy = ty * ds + ry * dx + uy * dy
        local wz = tz * ds + rz * dx + uz * dy
        local pitch, yaw = M.look(wx, wy, wz)
        transform.set_rotation(id, pitch, yaw, roll or 0)
    end
end

-- Distance between a segment (a moving bolt) and a point, both in rail coordinates.
function M.segment_distance(s0, x0, y0, s1, x1, y1, ps, px, py)
    local ds, dx, dy = s1 - s0, x1 - x0, y1 - y0
    local len2 = ds * ds + dx * dx + dy * dy
    local t = 0
    if len2 > 1e-9 then
        t = math.max(0, math.min(1, ((ps - s0) * ds + (px - x0) * dx + (py - y0) * dy) / len2))
    end
    local qs, qx, qy = s0 + ds * t - ps, x0 + dx * t - px, y0 + dy * t - py
    return math.sqrt(qs * qs + qx * qx + qy * qy)
end

function M.play(path)
    sound.play(path)
end

-- A burst of particles from one of the pooled emitters of a kind (`bursts`, `debris`, `sparks`).
function M.burst(kind, s, x, y, count)
    local S = M.state
    local pool = S[kind]
    if pool == nil or #pool == 0 then return end
    local key = kind .. "_next"
    local id = pool[S[key]]
    S[key] = S[key] % #pool + 1
    local wx, wy, wz = M.to_scene(s, x, y)
    transform.set_position(id, wx, wy, wz)
    entity.particles_emit(id, count)
end

-- Damage to the hull (none during the cutscenes and after the run is over).
function M.hurt(amount, source)
    local S = M.state
    if S.phase == "intro" or S.phase == "outro" or S.phase == "dead" then return end
    S.taken[source] = (S.taken[source] or 0) + 1
    S.hull = math.max(0, S.hull - amount)
    S.hurt_clock = 0.4
    M.burst("sparks", S.s, S.ship_x, S.ship_y, 20)
    M.play("sounds/game/hull_hit.wav")
end

function M.add_score(points)
    M.state.score = M.state.score + points
end

return M
