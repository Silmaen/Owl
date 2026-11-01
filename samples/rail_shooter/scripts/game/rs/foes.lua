-- The enemies: the waves of the data table, their flight patterns ahead of the carrier, their shots, the boss.
--
-- An enemy lives in the frame of the carrier (`dz` ahead of it, `x` across, `y` up) so that a squadron keeps pace with
-- the ship; its rail distance is the carrier's plus `dz`. Patterns: `line` and `vee` close in head on, `swoop_left` and
-- `swoop_right` cross the frame, `hover` holds ahead and strafes, `boss` holds ahead and weaves. Each kind has its hull,
-- its points and its rate of fire (a row of the `enemies` data table); their shots fly at the ship as it was when they
-- fired.

local rs = require("scripts.game.rs.core")
local fx = require("scripts.game.rs.fx")
local S, C = rs.state, rs.cfg

-- The explosion of each kind: its size (a fighter is 1) and the tint of its shards.
local BLASTS = {
    fighter = { size = 1.0, r = 0.8, g = 0.25, b = 0.22 },
    interceptor = { size = 1.5, r = 0.9, g = 0.58, b = 0.2 },
    boss = { size = 4.0, r = 0.6, g = 0.16, b = 0.22 },
}

local E = {}

-- The kinds of enemies, rows of the `enemies` table by key: prefab, hp, radius, points, reload.
local KINDS = {}

function E.load(table_path, kinds_path)
    KINDS = {}
    for _, key in ipairs(datatable.get_keys(kinds_path) or {}) do
        KINDS[key] = datatable.get_row(kinds_path, key)
    end
    S.wave_rows = {}
    for _, key in ipairs(datatable.get_keys(table_path) or {}) do
        local row = datatable.get_row(table_path, key)
        if row ~= nil then
            S.wave_rows[#S.wave_rows + 1] = row
            if row.kind == "boss" then S.boss_distance = row.distance end
        end
    end
    table.sort(S.wave_rows, function(a, b) return a.distance < b.distance end)
    S.next_wave = 1
end

local function spawn(row, index)
    local kind = KINDS[row.kind]
    if kind == nil then return nil end
    local id = scene.spawn_prefab(kind.prefab, rs.to_scene(S.s + 200, row.x, row.y))
    if id == 0 then return nil end
    local foe = {
        id = id, kind = row.kind, pattern = row.pattern, index = index, count = row.count, t = 0.0,
        hp = kind.hp, max_hp = kind.hp, radius = kind.radius, points = kind.points, reload = kind.reload,
        fire_clock = kind.reload * (0.6 + 0.25 * (index % 3)), base_x = row.x, base_y = row.y,
        dz = 200, x = row.x, y = row.y, roll = 0.0, alive = true, knock = 0.0, kick = { 0, 0, 0 },
    }
    S.enemies[#S.enemies + 1] = foe
    S.foes = S.foes + 1
    return foe
end

-- Spawn the waves the carrier reached; the boss row hands over to `rail.lua` through `on_boss`. A run started further
-- along the rail (the `startDistance` of the follower, to try a stretch in Owl Nest) skips the waves left behind.
function E.spawn_due(on_boss)
    while S.next_wave <= #S.wave_rows and S.wave_rows[S.next_wave].kind ~= "boss"
            and S.wave_rows[S.next_wave].distance < S.s - 60 do
        S.next_wave = S.next_wave + 1
    end
    while S.next_wave <= #S.wave_rows and S.s >= S.wave_rows[S.next_wave].distance do
        local row = S.wave_rows[S.next_wave]
        S.next_wave = S.next_wave + 1
        if row.kind == "boss" then
            S.boss = spawn(row, 0)
            on_boss()
        else
            for index = 0, row.count - 1 do spawn(row, index) end
        end
    end
end

-- Where a pattern puts its enemy at its age: dz, x, y, roll; done when it left the play.
local function fly(foe)
    local t, i, n = foe.t, foe.index, foe.count
    local mid = (n - 1) / 2
    local p = foe.pattern
    if p == "line" then
        return 150 - 14 * t, foe.base_x + (i - mid) * 4.5, foe.base_y + math.sin(t * 1.5 + i) * 1.0, 0
    elseif p == "vee" then
        return 150 - 14 * t + math.abs(i - mid) * 6, foe.base_x + (i - mid) * 3.5, foe.base_y + math.abs(i - mid) * 0.8, 0
    elseif p == "swoop_left" or p == "swoop_right" then
        local side = p == "swoop_left" and 1 or -1
        local u = math.max(0, t - i * 0.6)
        local x = side * (-30 + 13 * u)
        return 75 - 5 * u + i * 4, x, foe.base_y + 3 * math.sin(u * 1.1) - i * 0.8, -side * 0.6, math.abs(x) > 34 and u > 1
    elseif p == "hover" then
        local hold = 45 + i * 10
        local dz = math.max(hold, 170 - 28 * t)
        if t > 18 then dz = hold + (t - 18) * 35 end
        return dz, foe.base_x + (i - mid) * 8 + 6 * math.sin(0.7 * t + i), foe.base_y + 2 * math.sin(0.9 * t + i),
                -0.4 * math.cos(0.7 * t + i), dz > 230
    elseif p == "boss" then
        return math.max(C.boss_hold, 230 - 40 * t), 10 * math.sin(0.45 * t), foe.base_y + 3 * math.sin(0.7 * t),
                -0.25 * math.cos(0.45 * t)
    end
    return 150 - 14 * t, foe.base_x, foe.base_y, 0
end

-- Where the shots of the enemies go wide of the ship, in turn (no randomness: a run plays the same each time).
local MISS = { { 0, 0 }, { 0.9, 0.3 }, { 0, 0 }, { -0.7, 0.8 }, { 0.4, -0.9 }, { 0, 0 } }

local function shoot(foe, spread)
    S.shot_count = (S.shot_count or 0) + 1
    local miss = MISS[S.shot_count % #MISS + 1]
    local dz, dx, dy = -foe.dz, S.ship_x + miss[1] - foe.x, S.ship_y + miss[2] - foe.y
    local n = math.sqrt(dz * dz + dx * dx + dy * dy)
    if n < 1 then return end
    local speed = C.enemy_bolt_speed
    for k = -spread, spread do
        local ax = dx / n + k * 0.12
        local id = scene.spawn_prefab("prefabs/enemy_bolt.owlprefab", rs.to_scene(S.s + foe.dz, foe.x, foe.y))
        if id ~= 0 then
            S.enemy_bolts[#S.enemy_bolts + 1] = {
                id = id, dz = foe.dz, x = foe.x, y = foe.y, vz = dz / n * speed, vx = ax * speed, vy = dy / n * speed,
                life = 4.0,
            }
        end
    end
    rs.play("sounds/game/enemy_laser.wav")
end

function E.destroy(foe, by_player)
    foe.alive = false
    scene.destroy_entity(foe.id)
    if by_player then
        S.kills = S.kills + 1
        rs.add_score(foe.points)
        local blast = BLASTS[foe.kind] or BLASTS.fighter
        fx.explode(S.s + foe.dz, foe.x, foe.y, blast.size, blast.r, blast.g, blast.b)
        rs.play(foe.kind == "boss" and "sounds/game/boss_explosion.wav" or "sounds/game/explosion.wav")
    end
end

-- A bolt hit the enemy at a point (rail coordinates), flying along (ds, dx, dy): true when it was destroyed. A hit
-- that does not destroy it flashes it and knocks it back along the shot (for the eye only: its pattern goes on).
function E.hit(foe, is, ix, iy, ds, dx, dy)
    foe.hp = foe.hp - 1
    if foe.hp > 0 then
        rs.burst("sparks", is or S.s + foe.dz, ix or foe.x, iy or foe.y, 16)
        rs.play("sounds/game/hit.wav")
        fx.flash(foe.id, 5.0, 4.4, 3.8)
        local n = math.sqrt((ds or 1) ^ 2 + (dx or 0) ^ 2 + (dy or 0) ^ 2)
        local push = foe.kind == "boss" and 0.25 or 0.7
        foe.kick = { (ds or 1) / n * push, (dx or 0) / n * push, (dy or 0) / n * push }
        foe.knock = 0.25
        return false
    end
    E.destroy(foe, true)
    return true
end

function E.update(dt, may_fire)
    local kept = {}
    for _, foe in ipairs(S.enemies) do
        if foe.alive then
            foe.t = foe.t + dt
            local dz, x, y, roll, gone = fly(foe)
            foe.dz, foe.x, foe.y, foe.roll = dz, x, y, roll
            if gone or dz < -12 then
                E.destroy(foe, false)
            else
                -- Nose towards the ship, banked by the pattern; a knocked one jolts back along the shot and shakes.
                local ks, kx, ky, jolt = 0, 0, 0, 0
                if foe.knock > 0 then
                    foe.knock = math.max(0, foe.knock - dt)
                    local k = math.sin(foe.knock / 0.25 * math.pi)
                    ks, kx, ky = foe.kick[1] * k, foe.kick[2] * k, foe.kick[3] * k
                    jolt = math.sin(foe.knock * 90) * foe.knock * 0.8
                end
                rs.place(foe.id, S.s + dz + ks, x + kx, y + ky, -math.max(10, dz), S.ship_x - x, S.ship_y - y,
                        roll + jolt)
                foe.fire_clock = foe.fire_clock - dt
                if may_fire and foe.fire_clock <= 0 and dz > 25 and dz < 115 then
                    foe.fire_clock = foe.reload
                    shoot(foe, foe.kind == "boss" and (foe.t % 2 < 1 and 2 or 0) or 0)
                end
                -- Ramming the ship.
                local ddx, ddy = x - S.ship_x, y - S.ship_y
                local reach = foe.radius + C.ship_radius
                if math.abs(dz) < reach and ddx * ddx + ddy * ddy + dz * dz < reach * reach and foe.kind ~= "boss" then
                    E.destroy(foe, true)
                    rs.hurt(C.ram_damage, "ram")
                end
            end
        end
        if foe.alive then kept[#kept + 1] = foe end
    end
    S.enemies = kept
    if S.boss ~= nil and not S.boss.alive then S.boss_dead = true end
    E.update_bolts(dt)
end

function E.update_bolts(dt)
    local kept = {}
    for _, bolt in ipairs(S.enemy_bolts) do
        local z0, x0, y0 = bolt.dz, bolt.x, bolt.y
        bolt.dz, bolt.x, bolt.y = z0 + bolt.vz * dt, x0 + bolt.vx * dt, y0 + bolt.vy * dt
        bolt.life = bolt.life - dt
        local reach = C.ship_radius + 0.25
        local miss = rs.segment_distance(z0, x0, y0, bolt.dz, bolt.x, bolt.y, 0, S.ship_x, S.ship_y)
        local alive = bolt.life > 0 and bolt.dz > -15
        if alive and miss < reach and S.phase ~= "outro" and S.phase ~= "dead" then
            alive = false
            if S.roll_time > 0 then
                rs.burst("sparks", S.s + bolt.dz, bolt.x, bolt.y, 10)
                S.deflected = (S.deflected or 0) + 1
            else
                rs.hurt(C.bolt_damage, "bolt")
            end
        end
        if alive then
            rs.place(bolt.id, S.s + bolt.dz, bolt.x, bolt.y, bolt.vz, bolt.vx, bolt.vy)
            kept[#kept + 1] = bolt
        else
            scene.destroy_entity(bolt.id)
        end
    end
    S.enemy_bolts = kept
end

-- Everything still flying goes away (the boss down, the run over).
function E.clear()
    for _, foe in ipairs(S.enemies) do
        if foe.alive then E.destroy(foe, false) end
    end
    S.enemies = {}
    for _, bolt in ipairs(S.enemy_bolts) do scene.destroy_entity(bolt.id) end
    S.enemy_bolts = {}
end

return E
