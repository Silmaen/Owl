-- The fight of an Asteroid Field mission, for the graph `mission.owlvs`: the lasers in flight and what they hit, the
-- enemies of the waves and their hull, the convoy and its progress, the rocks the ships run into, the wingmen's
-- orders, the target lock, the aim of the guns and the lead of the aim assist. A graph would hide these lists, loops and
-- the intercept equation; the flying is the engine's (`ShipController`
-- for the player, `SteeringAgent` and `BehaviourTreeAgent` for the others).
--
-- The player's fighter bounces off the rocks and the other ships: pushed back out of the contact, its velocity along
-- the contact normal reflected with a floor speed, so a ram always ends in a visible rebound. The right button held
-- turns it toward the aim circle, the farther the circle from the centre the faster; its throttle is the
-- `ShipController`'s persistent one, cut by `throttle_zero`. A hit shows on what it hits: a flash of its model, sparks
-- and a glow at the impact, a sound, and on the player a shake of the camera (none with the `reduced_motion` setting)
-- and red screen edges; a kill throws a fireball, an expanding flash and debris; a damaged fighter or freighter smokes.
--
-- The enemies never stay inside a freighter: pushed back out of its hull, they break off when they come too close to
-- their target, and hold their fire at point blank, so the damage of a freighter comes from shots crossing the gap.
--
-- The convoy rides the `Route` spline (`SplineFollower`s); the waves come from `tables/waves.owltable`, a row per
-- wave: the distance of the convoy that brings it, how many fighters, of which kind, and whom they go for.

local M = {}

local KINDS = {
    fighter = { hull = 2, speed = 30, prefab = "prefabs/fighter.owlprefab", score = 100 },
    interceptor = { hull = 4, speed = 38, prefab = "prefabs/interceptor.owlprefab", score = 250 },
}
local BOLT = "prefabs/bolt.owlprefab"
local ENEMY_BOLT = "prefabs/enemy_bolt.owlprefab"
local ROCKS = 2 -- collision mask of the rocks (layer 1)
local BOLT_SPEED = { player = 160, wing = 160, enemy = 90 } -- speed of a laser from its gun, in metres per second
local CONVERGE = 150 -- the guns converge on the aim ray this far from the camera without a lead to aim at
local GIMBAL = math.cos(0.6) -- the guns turn up to 0.6 rad (34 degrees) off the nose
local LOCK_CONE = math.cos(0.2) -- a lock takes the enemy nearest the aim circle within 0.2 rad of it
local MOUSE_JUMP = 200 -- a mouse step longer than this, in pixels, is a jump of the cursor (a capture), not an aim
local PLAYER_RADIUS = 1.6 -- the player's hull against the rocks and the ships, in metres
local SHIP_RADIUS = { enemy = 1.4, wing = 1.6, freighter = 5 } -- the hull of the other ships, in metres
local ROCK_RADIUS = 0.8 -- radius of a rock's collider per unit of its scale (a 1.6 sphere)
local RESTITUTION = 0.7 -- share of the closing speed a rebound gives back
local BOUNCE_MIN = 9 -- slowest rebound off a contact, in metres per second
local RAM_SPEED = 10 -- closing speed above which a ship ram hurts both ships
local SHAKE_DECAY = 1.8 -- trauma of the camera shake lost per second
local STEER_FULL = 0.25 -- the aim circle this far from the centre, in screen heights, turns the fighter at full rate
local STEER_DEAD = 0.01 -- closer to the centre than this, in screen heights, the right button does not turn
local BREAK_OFF = 12 -- an enemy closer than this to the hull of its target breaks off, in metres
local EDGE_TIME = 0.18 -- seconds the red screen edges show after a hit on the player
local SMOKE_HULL = { player = 35, freighter = 60 } -- hull under which a ship smokes

local S = {
    player = 0, camera = 0, route = 0, gate = 0, length = 1,
    hull = 100, hull_max = 100, kills = 0, shots = 0, hits = 0, enemy_shots = 0,
    freighters = {}, wings = {}, enemies = {}, bolts = {},
    waves = {}, next_wave = 1, wave = 0,
    fire_clock = 0, hurt_clock = 0, lock = 0, order = "escort",
    bursts = {}, burst_next = 1,
    -- The aim circle, in viewport pixels from the centre, the last mouse position, the widgets and the lead.
    aim_x = 0, aim_y = 0, mouse_x = nil, mouse_y = nil, circle = 0, lead_mark = 0, lead = nil, lock_cycles = 0,
    -- The hit effects: the player's model and camera, the spark emitters, the flashing tints, the shake, the rebounds.
    model = 0, camera_base = { 0, 0, 0 }, sparks = {}, spark_next = 1, tints = {}, trauma = 0, still = false,
    bounces = 0, models = {}, shake_clock = 0,
    flashes = {}, flash_next = 1, debris = {}, debris_next = 1, glows = {}, glow_next = 1,
    edges = {}, edge_clock = 0, smoking = false, throttle_text = 0, throttle_bar = 0, throttle_shown = nil,
}

local function length(x, y, z)
    return math.sqrt(x * x + y * y + z * z)
end

local function distance(a, b)
    local ax, ay, az = transform.get_world_position(a)
    local bx, by, bz = transform.get_world_position(b)
    return length(ax - bx, ay - by, az - bz)
end

-- Forward axis (-Z) of an entity from its Euler angles (Rz * Ry * Rx).
local function forward(id)
    local rx, ry, rz = transform.get_rotation(id)
    local ca, sa = math.cos(rx), math.sin(rx)
    local cb, sb = math.cos(ry), math.sin(ry)
    local cc, sc = math.cos(rz), math.sin(rz)
    return -(ca * sb * cc + sa * sc), -(ca * sb * sc - sa * cc), -(ca * cb)
end

local function burst(x, y, z, big)
    local id = S.bursts[S.burst_next]
    if id == nil or id == 0 then return end
    S.burst_next = S.burst_next % #S.bursts + 1
    transform.set_position(id, x, y, z)
    entity.particles_emit(id, big and 120 or 50)
end

local function spark(x, y, z, count)
    local id = S.sparks[S.spark_next]
    if id == nil or id == 0 then return end
    S.spark_next = S.spark_next % #S.sparks + 1
    transform.set_position(id, x, y, z)
    entity.particles_emit(id, count or 24)
end

-- The next emitter of a pool (the flashes, the debris, the glows) placed at a point and fired.
local function pooled(pool, key, x, y, z, count)
    local id = S[pool][S[key]]
    if id == nil or id == 0 then return end
    S[key] = S[key] % #S[pool] + 1
    transform.set_position(id, x, y, z)
    entity.particles_emit(id, count)
end

-- An explosion: the fireball, the sparks, the expanding flash and the debris; `big` for a freighter.
local function explode(x, y, z, big)
    burst(x, y, z, true)
    spark(x, y, z, big and 64 or 44)
    pooled("flashes", "flash_next", x, y, z, big and 3 or 1)
    pooled("debris", "debris_next", x, y, z, big and 90 or 30)
    if big then
        burst(x + 3, y + 1, z - 2, true)
        burst(x - 3, y - 1, z + 2, false)
    end
end

-- A small hot glow where a shot meets a hull.
local function glow(x, y, z, count)
    pooled("glows", "glow_next", x, y, z, count or 1)
end

-- A hit flash on a model: its tint jumps to a bright colour and fades back to the one it had.
local function flash(id, r, g, b)
    if id == nil or id == 0 then return end
    local tint = S.tints[id]
    if tint == nil then
        local br, bg, bb, ba = entity.get_tint(id)
        if br == nil then return end
        tint = { base = { br, bg, bb, ba } }
        S.tints[id] = tint
    end
    tint.colour, tint.t, tint.life = { r, g, b }, 0, 0.15
    entity.set_tint(id, r, g, b, tint.base[4])
end

local function shake(amount)
    if S.still then return end
    S.trauma = math.min(1, S.trauma + amount)
end

-- The flashes fading, the camera shaking about its place behind the fighter.
local function update_effects(dt)
    for id, tint in pairs(S.tints) do
        tint.t = tint.t + dt
        local k = math.max(0, 1 - tint.t / tint.life)
        local base, c = tint.base, tint.colour
        local alive = entity.set_tint(id, base[1] + (c[1] - base[1]) * k, base[2] + (c[2] - base[2]) * k,
                base[3] + (c[3] - base[3]) * k, base[4])
        if not alive or k <= 0 then S.tints[id] = nil end
    end
    if S.edge_clock > 0 then
        S.edge_clock = S.edge_clock - dt
        if S.edge_clock <= 0 then
            for _, id in ipairs(S.edges) do ui.set_visible(id, false) end
        end
    end
    if S.camera == 0 or S.trauma <= 0 then return end
    local b = S.camera_base
    S.trauma = math.max(0, S.trauma - SHAKE_DECAY * dt)
    if S.trauma <= 0 then
        transform.set_position(S.camera, b[1], b[2], b[3])
        return
    end
    local k = S.trauma * S.trauma * 0.6
    S.shake_clock = S.shake_clock + dt
    local t = S.shake_clock
    transform.set_position(S.camera, b[1] + k * (math.sin(t * 47) + 0.5 * math.sin(t * 89 + 1.3)),
            b[2] + k * (math.sin(t * 53 + 2.1) + 0.5 * math.sin(t * 97)), b[3])
end

-- A laser: from a ship along a direction; `team` is "player", "wing" or "enemy". The player's lasers also carry the
-- velocity of the fighter (`vx, vy, vz`), the others leave at their gun speed.
local function shoot(from, dx, dy, dz, team, vx, vy, vz)
    local x, y, z = transform.get_world_position(from)
    local id = scene.spawn_prefab(team == "enemy" and ENEMY_BOLT or BOLT, x + dx * 3, y + dy * 3, z + dz * 3)
    if id == 0 then return end
    local speed = BOLT_SPEED[team] or BOLT_SPEED.player
    vx, vy, vz = dx * speed + (vx or 0), dy * speed + (vy or 0), dz * speed + (vz or 0)
    local v = length(vx, vy, vz)
    local ux, uy, uz = vx / v, vy / v, vz / v
    transform.set_rotation(id, math.asin(math.max(-1, math.min(1, uy))), math.atan(-ux, -uz), 0)
    S.bolts[#S.bolts + 1] = { id = id, dx = ux, dy = uy, dz = uz, speed = v, life = 1.6, team = team, from = from }
    sound.play(team == "enemy" and "sounds/game/enemy_laser.wav" or "sounds/game/laser.wav")
end

-- Distance from a point to a segment.
local function segment_distance(px, py, pz, ax, ay, az, bx, by, bz)
    local vx, vy, vz = bx - ax, by - ay, bz - az
    local wx, wy, wz = px - ax, py - ay, pz - az
    local vv = vx * vx + vy * vy + vz * vz
    local t = vv > 0 and math.max(0, math.min(1, (wx * vx + wy * vy + wz * vz) / vv)) or 0
    return length(ax + vx * t - px, ay + vy * t - py, az + vz * t - pz)
end

local function kill_enemy(index)
    local enemy = table.remove(S.enemies, index)
    local x, y, z = transform.get_world_position(enemy.id)
    explode(x, y, z, false)
    S.tints[enemy.model] = nil
    local near = distance(S.player, enemy.id)
    if near < 60 then shake(0.35 * (1 - near / 60) + 0.1) end
    sound.play("sounds/game/explosion.wav")
    if S.lock == enemy.id then S.lock = 0 end
    scene.destroy_entity(enemy.id)
    S.kills = S.kills + 1
    gamestate.set("field_score", gamestate.get("field_score", 0) + KINDS[enemy.kind].score)
end

local function hurt_player(amount)
    S.hull = math.max(0, S.hull - amount)
    S.hurt_clock = 0
    sound.play("sounds/game/hull_hit.wav")
    flash(S.model, 4.0, 1.2, 1.0)
    local x, y, z = transform.get_world_position(S.player)
    spark(x, y, z, 30)
    glow(x, y, z, 2)
    shake(0.2 + amount / 40)
    if #S.edges > 0 then
        for _, id in ipairs(S.edges) do ui.set_visible(id, true) end
        S.edge_clock = EDGE_TIME
    end
    if not S.smoking and S.hull < SMOKE_HULL.player and S.player_smoke ~= 0 then
        S.smoking = entity.particles_play(S.player_smoke)
    end
    gamestate.set("field_hurt", gamestate.get("field_hurt", 0) + 1)
end

local function hurt_freighter(freighter, amount)
    if freighter.hull <= 0 then return end
    freighter.hull = math.max(0, freighter.hull - (S.sturdy and amount / 2 or amount))
    flash(freighter.model, 3.0, 2.2, 1.8)
    if not freighter.smoking and freighter.hull < SMOKE_HULL.freighter and freighter.smoke ~= 0 then
        freighter.smoking = entity.particles_play(freighter.smoke)
    end
    if freighter.hull <= 0 then
        local x, y, z = transform.get_world_position(freighter.id)
        explode(x, y, z, true)
        if freighter.smoke ~= 0 then entity.particles_stop(freighter.smoke) end
        if distance(S.player, freighter.id) < 120 then shake(0.4) end
        sound.play("sounds/game/explosion.wav")
        ui.show_subtitle("[a freighter explodes]", 2)
        entity.set_spline_follower_enabled(freighter.id, false)
        ui.set_visible(freighter.id, false)
        entity.set_hud_marker_enabled(freighter.id, false)
    end
end

-- What a bolt met over its step, if anything: a rock, an enemy, a freighter, the player or a wingman.
local function strike(bolt, ax, ay, az, bx, by, bz, step)
    local hit = physics.raycast_3d(ax, ay, az, bolt.dx, bolt.dy, bolt.dz, step, ROCKS)
    if hit ~= nil then
        burst(hit.x, hit.y, hit.z, false)
        glow(hit.x, hit.y, hit.z)
        sound.play("sounds/game/rock_hit.wav")
        return true
    end
    if bolt.team ~= "enemy" then
        for index, enemy in ipairs(S.enemies) do
            local ex, ey, ez = transform.get_world_position(enemy.id)
            if segment_distance(ex, ey, ez, ax, ay, az, bx, by, bz) < 2.2 then
                enemy.hull = enemy.hull - 1
                if bolt.from == S.player then S.hits = S.hits + 1 end
                sound.play("sounds/game/hit.wav")
                flash(enemy.model, 5.0, 4.4, 3.8)
                spark(ex - bolt.dx, ey - bolt.dy, ez - bolt.dz, 30)
                glow(ex - bolt.dx, ey - bolt.dy, ez - bolt.dz)
                if enemy.hull <= 0 then
                    kill_enemy(index)
                elseif enemy.hull <= 1 then
                    bt.set_value(enemy.id, "hurt", true)
                end
                return true
            end
        end
        return false
    end
    for _, freighter in ipairs(S.freighters) do
        if freighter.hull > 0 then
            local fx, fy, fz = transform.get_world_position(freighter.id)
            if segment_distance(fx, fy, fz, ax, ay, az, bx, by, bz) < SHIP_RADIUS.freighter then
                -- The impact on the hull, on the side the shot comes from.
                local nx, ny, nz = ax - fx, ay - fy, az - fz
                local l = math.max(1e-4, length(nx, ny, nz))
                local r = SHIP_RADIUS.freighter * 0.8
                local hx, hy, hz = fx + nx / l * r, fy + ny / l * r, fz + nz / l * r
                spark(hx, hy, hz, 26)
                glow(hx, hy, hz, 2)
                hurt_freighter(freighter, 3)
                return true
            end
        end
    end
    local px, py, pz = transform.get_world_position(S.player)
    if segment_distance(px, py, pz, ax, ay, az, bx, by, bz) < 1.8 then
        hurt_player(6)
        return true
    end
    return false
end

local function move_bolts(dt)
    local index = 1
    while index <= #S.bolts do
        local bolt = S.bolts[index]
        local ax, ay, az = transform.get_world_position(bolt.id)
        local step = bolt.speed * dt
        local bx, by, bz = ax + bolt.dx * step, ay + bolt.dy * step, az + bolt.dz * step
        bolt.life = bolt.life - dt
        if strike(bolt, ax, ay, az, bx, by, bz, step) or bolt.life <= 0 then
            scene.destroy_entity(bolt.id)
            table.remove(S.bolts, index)
        else
            transform.set_position(bolt.id, bx, by, bz)
            index = index + 1
        end
    end
end

-- The player's fighter against one obstacle at (x, y, z), moving at (ox, oy, oz), their hulls touching at `reach`:
-- pushed back out along the contact normal, its velocity along the normal reflected (at least `BOUNCE_MIN` outward).
-- Returns the closing speed of the impact, nil without contact.
local function bounce(x, y, z, reach, ox, oy, oz)
    local px, py, pz = transform.get_world_position(S.player)
    local nx, ny, nz = px - x, py - y, pz - z
    local d = length(nx, ny, nz)
    if d >= reach then return nil end
    if d < 1e-4 then
        nx, ny, nz, d = 0, 1, 0, 1
    end
    nx, ny, nz = nx / d, ny / d, nz / d
    transform.set_world_position(S.player, x + nx * reach, y + ny * reach, z + nz * reach)
    local vx, vy, vz = entity.get_ship_velocity(S.player)
    if vx == nil then return 0 end
    local rx, ry, rz = vx - (ox or 0), vy - (oy or 0), vz - (oz or 0)
    local along = rx * nx + ry * ny + rz * nz
    local change = math.max(BOUNCE_MIN, -RESTITUTION * along) - along
    entity.set_ship_velocity(S.player, vx + nx * change, vy + ny * change, vz + nz * change)
    S.bounces = S.bounces + 1
    gamestate.set("field_bounces", S.bounces)
    spark(px - nx * PLAYER_RADIUS, py - ny * PLAYER_RADIUS, pz - nz * PLAYER_RADIUS, 28)
    shake(0.25)
    return math.max(0, -along)
end

-- Ships running into the rocks and into each other: the player bounces off (a rock costs hull, a hard ram costs both
-- ships), an enemy that runs into a rock blows up.
local function rams()
    local px, py, pz = transform.get_world_position(S.player)
    for _, rock in ipairs(physics.overlap_sphere(px, py, pz, PLAYER_RADIUS, ROCKS)) do
        local rx, ry, rz = transform.get_world_position(rock)
        local scale = transform.get_scale(rock)
        local closing = bounce(rx, ry, rz, (scale or 1) * ROCK_RADIUS + PLAYER_RADIUS)
        if closing ~= nil and S.hurt_clock > 0.6 then
            hurt_player(15)
            sound.play("sounds/game/rock_hit.wav")
        end
    end
    local index = 1
    while index <= #S.enemies do
        local enemy = S.enemies[index]
        local ex, ey, ez = transform.get_world_position(enemy.id)
        local vx, vy, vz = steer.get_velocity(enemy.id)
        local closing = bounce(ex, ey, ez, SHIP_RADIUS.enemy + PLAYER_RADIUS, vx, vy, vz)
        if closing ~= nil and closing > RAM_SPEED and S.hurt_clock > 0.6 then
            hurt_player(4)
            flash(enemy.model, 5.0, 4.4, 3.8)
            enemy.hull = enemy.hull - 1
        end
        if enemy.hull <= 0 or #physics.overlap_sphere(ex, ey, ez, 1.4, ROCKS) > 0 then
            kill_enemy(index)
        else
            index = index + 1
        end
    end
    for _, wing in ipairs(S.wings) do
        local wx, wy, wz = transform.get_world_position(wing)
        local vx, vy, vz = steer.get_velocity(wing)
        bounce(wx, wy, wz, SHIP_RADIUS.wing + PLAYER_RADIUS, vx, vy, vz)
    end
    for _, freighter in ipairs(S.freighters) do
        if freighter.hull > 0 then
            local fx, fy, fz = transform.get_world_position(freighter.id)
            local closing = bounce(fx, fy, fz, SHIP_RADIUS.freighter + PLAYER_RADIUS)
            if closing ~= nil and closing > RAM_SPEED then sound.play("sounds/game/rock_hit.wav") end
        end
    end
end

-- The enemies against the freighters: pushed back out of a hull they enter, and sent off when they close in on their
-- target (the break-off branch of `enemy.owlbt`), so they make passes instead of sitting in it.
local function keep_out(dt)
    local reach = SHIP_RADIUS.freighter + SHIP_RADIUS.enemy
    for _, enemy in ipairs(S.enemies) do
        local ex, ey, ez = transform.get_world_position(enemy.id)
        local hugging = false
        for _, freighter in ipairs(S.freighters) do
            if freighter.hull > 0 then
                local fx, fy, fz = transform.get_world_position(freighter.id)
                local nx, ny, nz = ex - fx, ey - fy, ez - fz
                local d = length(nx, ny, nz)
                if d < reach then
                    if d < 1e-4 then nx, ny, nz, d = 0, 1, 0, 1 end
                    ex, ey, ez = fx + nx / d * reach, fy + ny / d * reach, fz + nz / d * reach
                    transform.set_world_position(enemy.id, ex, ey, ez)
                    S.pushed = S.pushed + 1
                end
                if d < reach + 3 then hugging = true end
                if freighter.id == enemy.target and d < SHIP_RADIUS.freighter + BREAK_OFF then
                    bt.set_value(enemy.id, "hurt", true)
                end
            end
        end
        -- How long it has stayed against a hull, the longest of the fight kept for the tests.
        enemy.hugging = hugging and (enemy.hugging or 0) + dt or 0
        S.hug_max = math.max(S.hug_max, enemy.hugging)
    end
    gamestate.set("field_pushed", S.pushed)
    gamestate.set("field_hug_time", S.hug_max)
end

local function spawn_wave(row)
    S.wave = S.wave + 1
    local lead = M.lead()
    local count = math.floor(row.count)
    for i = 1, count do
        -- Ahead of the convoy, spread on a ring across the route.
        local d = math.min(S.length, spline.get_distance(lead.id) + row.ahead)
        local x, y, z = spline.sample(S.route, d)
        local angle = (i / count) * math.pi * 2
        local kind = KINDS[row.kind] and row.kind or "fighter"
        local id = scene.spawn_prefab(KINDS[kind].prefab, x + math.cos(angle) * 40, y + math.sin(angle) * 25, z)
        if id ~= 0 then
            -- The model of this enemy is the one model of the scene no enemy has taken yet.
            local model = 0
            for _, candidate in ipairs(scene.find_entities("EnemyModel")) do
                if not S.models[candidate] then model = candidate end
            end
            if model ~= 0 then S.models[model] = true end
            local target = row.target == "player" and S.player or M.weakest()
            steer.set_target(id, target)
            steer.set_max_speed(id, KINDS[kind].speed)
            S.enemies[#S.enemies + 1] = {
                id = id, kind = kind, hull = KINDS[kind].hull, target = target, ready = false, model = model,
            }
        end
    end
    sound.play("sounds/game/alarm.wav")
    ui.show_subtitle("[enemy fighters inbound]", 2)
end

--- Read the scene: the player, the wingmen, the freighters on the route, the gate, the waves of a data table and
--- the explosion emitters.
---@param waves string
---@param sturdy boolean
---@return integer freighters
function M.setup(waves, sturdy)
    S.sturdy = sturdy
    S.player = scene.find_entity("Player")
    S.route = scene.find_entity("Route")
    S.gate = scene.find_entity("Gate")
    S.length = math.max(1, spline.get_length(S.route))
    S.freighters, S.wings, S.enemies, S.bolts, S.bursts = {}, {}, {}, {}, {}
    for i = 1, 3 do
        local id = scene.find_entity("Freighter" .. i)
        if id ~= 0 then
            S.freighters[#S.freighters + 1] = { id = id, hull = 100, model = scene.find_entity("Freighter" .. i .. "Model") }
        end
        local wing = scene.find_entity("Wing" .. i)
        if wing ~= 0 then S.wings[#S.wings + 1] = wing end
    end
    for index, freighter in ipairs(S.freighters) do
        freighter.smoke, freighter.smoking = scene.find_entity("Freighter" .. index .. "Smoke"), false
    end
    for i = 1, 6 do
        local id = scene.find_entity("Burst" .. i)
        if id ~= 0 then S.bursts[#S.bursts + 1] = id end
    end
    S.sparks, S.spark_next, S.tints, S.models, S.trauma, S.bounces = {}, 1, {}, {}, 0, 0
    for i = 1, 4 do
        local id = scene.find_entity("Spark" .. i)
        if id ~= 0 then S.sparks[#S.sparks + 1] = id end
    end
    S.flashes, S.debris, S.glows, S.flash_next, S.debris_next, S.glow_next = {}, {}, {}, 1, 1, 1
    for i = 1, 4 do
        for pool, name in pairs({ flashes = "Flash", debris = "Debris", glows = "Glow" }) do
            local id = scene.find_entity(name .. i)
            if id ~= 0 then S[pool][#S[pool] + 1] = id end
        end
    end
    S.player_smoke, S.smoking, S.pushed, S.hug_max = scene.find_entity("PlayerSmoke"), false, 0, 0
    S.edges, S.edge_clock = {}, 0
    for _, name in ipairs({ "HitTop", "HitBottom", "HitLeft", "HitRight" }) do
        local id = ui.find(name)
        if id ~= 0 then S.edges[#S.edges + 1] = id end
    end
    S.throttle_text, S.throttle_bar, S.throttle_shown = ui.find("ThrottleText"), ui.find("ThrottleBar"), nil
    S.model, S.camera = scene.find_entity("PlayerModel"), scene.find_entity("PlayerCamera")
    if S.camera ~= 0 then
        local cx, cy, cz = transform.get_position(S.camera)
        S.camera_base = { cx or 0, cy or 0, cz or 0 }
    end
    S.still = settings.get("reduced_motion", false) == true
    gamestate.set("field_bounces", 0)
    gamestate.set("field_hurt", 0)
    -- The aim circle is the cursor of the flight: the system one shows in the pause window only.
    input.set_cursor_visible(false)
    S.waves = {}
    for _, key in ipairs(datatable.get_keys(waves)) do
        S.waves[#S.waves + 1] = datatable.get_row(waves, key)
    end
    table.sort(S.waves, function(a, b) return a.distance < b.distance end)
    S.next_wave, S.wave, S.kills, S.shots, S.hits, S.hull = 1, 0, 0, 0, 0, S.hull_max
    S.circle, S.lead_mark = ui.find("AimCircle"), scene.find_entity("LeadMark")
    S.aim_x, S.aim_y, S.mouse_x, S.mouse_y, S.lead, S.lock, S.lock_cycles = 0, 0, nil, nil, nil, 0, 0
    if S.lead_mark ~= 0 then entity.set_hud_marker_enabled(S.lead_mark, false) end
    M.order("escort")
    return #S.freighters
end

--- The pause window opens or closes: the system cursor shows over the window and the fighter lets the cursor go (the
--- buttons take clicks); back in flight the aim circle is the only cursor.
---@param paused boolean
function M.pause_view(paused)
    input.set_cursor_visible(paused)
    entity.set_ship_enabled(S.player, not paused)
end

--- The lead freighter still flying (the first one otherwise).
---@return entity lead
function M.lead_id()
    return M.lead().id
end

-- The lead freighter still flying, as its entry of the convoy.
function M.lead()
    for _, freighter in ipairs(S.freighters) do
        if freighter.hull > 0 then return freighter end
    end
    return S.freighters[1]
end

--- The freighter with the least hull, the one the enemies go for.
---@return entity freighter
function M.weakest()
    local best, hull = S.player, 1e9
    for _, freighter in ipairs(S.freighters) do
        if freighter.hull > 0 and freighter.hull < hull then best, hull = freighter.id, freighter.hull end
    end
    return best
end

local function viewport()
    local w, h = scene.get_viewport_size()
    if w == nil or w <= 0 or h <= 0 then return 1280, 720 end
    return w, h
end

-- The ray from the camera through the aim circle: origin and unit direction.
local function aim_ray()
    local w, h = viewport()
    local ox, oy, oz, dx, dy, dz = scene.get_view_ray(w / 2 + S.aim_x, h / 2 + S.aim_y)
    if ox == nil then
        local px, py, pz = transform.get_world_position(S.player)
        local fx, fy, fz = forward(S.player)
        return px, py, pz, fx, fy, fz
    end
    return ox, oy, oz, dx, dy, dz
end

-- Where the guns must point for a laser to meet the locked enemy, from the fighter's point of view: the player's
-- lasers carry the fighter's velocity, so in the fighter's frame a laser flies straight at its gun speed `s` and the
-- target moves at its velocity minus the fighter's, `w`. The first time `t` with |r + w t| = s t solves
-- (w.w - s^2) t^2 + 2 (r.w) t + r.r = 0; the aim point is the fighter's position plus r + w t, nil without a positive
-- root.
local function lead_point()
    if S.lock == 0 then return nil end
    local px, py, pz = transform.get_world_position(S.player)
    local tx, ty, tz = transform.get_world_position(S.lock)
    local vx, vy, vz = steer.get_velocity(S.lock)
    local sx, sy, sz = entity.get_ship_velocity(S.player)
    local rx, ry, rz = tx - px, ty - py, tz - pz
    local wx, wy, wz = (vx or 0) - (sx or 0), (vy or 0) - (sy or 0), (vz or 0) - (sz or 0)
    local s = BOLT_SPEED.player
    local a = wx * wx + wy * wy + wz * wz - s * s
    local b = 2 * (rx * wx + ry * wy + rz * wz)
    local c = rx * rx + ry * ry + rz * rz
    local t
    if math.abs(a) < 1e-6 then
        if b < 0 then t = -c / b end
    else
        local disc = b * b - 4 * a * c
        if disc < 0 then return nil end
        local root = math.sqrt(disc)
        local t1, t2 = (-b - root) / (2 * a), (-b + root) / (2 * a)
        if t1 > t2 then t1, t2 = t2, t1 end
        t = t1 > 0 and t1 or (t2 > 0 and t2 or nil)
    end
    if t == nil then return nil end
    return px + rx + wx * t, py + ry + wy * t, pz + rz + wz * t
end

-- Show the X of the aim assist on the lead point, hidden without a solution or behind the camera.
local function place_lead()
    local x, y, z = lead_point()
    local w, h = viewport()
    local ox, oy, oz, dx, dy, dz = scene.get_view_ray(w / 2, h / 2)
    local ahead = x ~= nil and (ox == nil or (x - ox) * dx + (y - oy) * dy + (z - oz) * dz > 0)
    S.lead = ahead and { x = x, y = y, z = z } or nil
    if S.lead_mark ~= 0 then
        if ahead then transform.set_position(S.lead_mark, x, y, z) end
        entity.set_hud_marker_enabled(S.lead_mark, ahead)
    end
    gamestate.set("field_lead", ahead)
end

-- The direction of the guns: from the fighter to the point of the aim ray they converge on (the lead point when the
-- aim circle is on it), turned back into the gimbal cone around the nose.
local function gun_direction()
    local ox, oy, oz, dx, dy, dz = aim_ray()
    local range = CONVERGE
    if S.lead ~= nil then range = length(S.lead.x - ox, S.lead.y - oy, S.lead.z - oz) end
    local px, py, pz = transform.get_world_position(S.player)
    local gx, gy, gz = ox + dx * range - px, oy + dy * range - py, oz + dz * range - pz
    local l = length(gx, gy, gz)
    local fx, fy, fz = forward(S.player)
    if l <= 0 then return fx, fy, fz end
    gx, gy, gz = gx / l, gy / l, gz / l
    local along = gx * fx + gy * fy + gz * fz
    if along >= GIMBAL then return gx, gy, gz end
    local qx, qy, qz = gx - fx * along, gy - fy * along, gz - fz * along
    local q = length(qx, qy, qz)
    if q <= 1e-6 then return fx, fy, fz end
    local sine = math.sqrt(1 - GIMBAL * GIMBAL)
    return fx * GIMBAL + qx / q * sine, fy * GIMBAL + qy / q * sine, fz * GIMBAL + qz / q * sine
end

-- The share of the turn rate for an offset of the aim circle, in screen heights: proportional up to `STEER_FULL`.
local function steer_share(offset)
    local size = math.abs(offset)
    if size < STEER_DEAD then return 0 end
    local share = math.min(1, (size - STEER_DEAD) / (STEER_FULL - STEER_DEAD))
    return offset < 0 and -share or share
end

-- The right button held turns the fighter toward the aim circle; `throttle_zero` cuts the throttle, the HUD shows it.
local function pilot(h)
    local steering = input.is_action_pressed("steer") and S.hull > 0
    if steering then entity.set_ship_turn(S.player, steer_share(S.aim_x / h), steer_share(S.aim_y / h)) end
    gamestate.set("field_steering", steering)
    if input.is_action_just_pressed("throttle_zero") then entity.set_ship_throttle(S.player, 0) end
    local throttle = entity.get_ship_throttle(S.player) or 0
    gamestate.set("field_throttle", throttle)
    local percent = math.floor(math.abs(throttle) * 100 + 0.5)
    if percent == S.throttle_shown then return end
    S.throttle_shown = percent
    if S.throttle_text ~= 0 then
        ui.set_text(S.throttle_text, (throttle < 0 and "REVERSE " or "THROTTLE ") .. percent .. "%")
    end
    if S.throttle_bar ~= 0 then ui.set_progress(S.throttle_bar, math.abs(throttle)) end
end

local function set_lock(id)
    if S.lock ~= 0 then entity.set_hud_marker_label(S.lock, "") end
    S.lock = id
    if id ~= 0 then entity.set_hud_marker_label(id, "LOCKED") end
end

--- One tick of the fight: the player's guns (held `fire`, toward the aim circle), the lasers in flight, the rams, the
--- waves the convoy reaches.
---@param fire boolean
---@param dt number
function M.tick(fire, dt)
    S.fire_clock = S.fire_clock - dt
    S.hurt_clock = S.hurt_clock + dt
    if fire and S.fire_clock <= 0 and S.hull > 0 then
        local dx, dy, dz = gun_direction()
        local vx, vy, vz = entity.get_ship_velocity(S.player)
        shoot(S.player, dx, dy, dz, "player", vx, vy, vz)
        S.shots = S.shots + 1
        S.fire_clock = 0.14
    end
    move_bolts(dt)
    rams()
    keep_out(dt)
    update_effects(dt)
    -- A tree starts at the tick after its spawn: its target is written once it runs.
    for _, enemy in ipairs(S.enemies) do
        if not enemy.ready then enemy.ready = bt.set_value(enemy.id, "target", enemy.target) end
    end
    local row = S.waves[S.next_wave]
    if row ~= nil and spline.get_distance(M.lead().id) >= row.distance then
        S.next_wave = S.next_wave + 1
        spawn_wave(row)
    end
end

--- Lock the enemy nearest the aim circle; with none near it, drop the lock. False when nothing is locked.
---@return boolean locked
function M.lock()
    local ox, oy, oz, dx, dy, dz = aim_ray()
    local best, score = 0, LOCK_CONE
    for _, enemy in ipairs(S.enemies) do
        local ex, ey, ez = transform.get_world_position(enemy.id)
        local lx, ly, lz = ex - ox, ey - oy, ez - oz
        local l = length(lx, ly, lz)
        local facing = l > 0 and (lx * dx + ly * dy + lz * dz) / l or 0
        if facing > score then best, score = enemy.id, facing end
    end
    set_lock(best)
    place_lead()
    return best ~= 0
end

--- Move the lock to the next enemy (`step` 1) or the previous one (-1), in the order they came; the first one without
--- a lock. False when there is no enemy.
---@param step integer
---@return boolean locked
function M.cycle_lock(step)
    local count = #S.enemies
    if count == 0 then
        set_lock(0)
        return false
    end
    local index = 0
    for i, enemy in ipairs(S.enemies) do
        if enemy.id == S.lock then index = i end
    end
    if index == 0 then
        index = step >= 0 and 1 or count
    else
        index = (index - 1 + step) % count + 1
    end
    set_lock(S.enemies[index].id)
    S.lock_cycles = S.lock_cycles + 1
    gamestate.set("field_lock_cycles", S.lock_cycles)
    sound.play("sounds/game/lock.wav")
    return true
end

--- Each frame of the flight: the mouse moves the aim circle (a cursor of its own, kept in the view, so it works with
--- the cursor captured), the right button held turns the fighter toward it, the wheel or the lock buttons move the
--- lock along the enemies, the X of the aim assist follows the lead of the locked enemy, the HUD shows the throttle.
--- Returns whether an enemy is locked.
---@param mouse_x number
---@param mouse_y number
---@param wheel number
---@param next_lock boolean
---@param previous_lock boolean
---@return boolean locked
function M.aim(mouse_x, mouse_y, wheel, next_lock, previous_lock)
    local w, h = viewport()
    if S.mouse_x ~= nil then
        local mx, my = mouse_x - S.mouse_x, mouse_y - S.mouse_y
        if length(mx, my, 0) < MOUSE_JUMP then S.aim_x, S.aim_y = S.aim_x + mx, S.aim_y + my end
    end
    S.mouse_x, S.mouse_y = mouse_x, mouse_y
    local margin = h * 0.04
    S.aim_x = math.max(margin - w / 2, math.min(w / 2 - margin, S.aim_x))
    S.aim_y = math.max(margin - h / 2, math.min(h / 2 - margin, S.aim_y))
    if S.circle ~= 0 then ui.set_offset(S.circle, S.aim_x / h, S.aim_y / h) end
    gamestate.set("field_aim_x", S.aim_x)
    gamestate.set("field_aim_y", S.aim_y)
    -- The wheel down (a negative offset) or the next button goes on, up or the previous button goes back.
    local step = (next_lock and 1 or 0) - (previous_lock and 1 or 0)
    if wheel < 0 then step = step + 1 elseif wheel > 0 then step = step - 1 end
    if step ~= 0 then M.cycle_lock(step) end
    pilot(h)
    place_lead()
    return S.lock ~= 0
end

--- Order the wingmen: `attack` (each goes for the enemies) or `escort` (back in formation on the player).
---@param kind string
---@return string order
function M.order(kind)
    S.order = kind == "attack" and "attack" or "escort"
    for _, wing in ipairs(S.wings) do bt.set_value(wing, "order", S.order) end
    return S.order
end

--- Behaviour tree leaf of an enemy: fire at its target when it is ahead.
---@param ship entity
---@return boolean fired
function M.enemy_fire(ship)
    local target = bt.get_value(ship, "target")
    if target == nil or target == 0 then return false end
    local sx, sy, sz = transform.get_world_position(ship)
    local tx, ty, tz = transform.get_world_position(target)
    local lx, ly, lz = tx - sx, ty - sy, tz - sz
    local l = length(lx, ly, lz)
    -- At point blank the laser would start inside the target's hull: no shot.
    local hull = target == S.player and PLAYER_RADIUS or SHIP_RADIUS.freighter
    if l <= hull + 4 then return false end
    shoot(ship, lx / l, ly / l, lz / l, "enemy")
    S.enemy_shots = S.enemy_shots + 1
    return true
end

--- Behaviour tree leaf of a wingman: pick the enemy nearest to it as its target; false when there is none.
---@param ship entity
---@return boolean found
function M.wingman_pick(ship)
    local best, near = 0, 1e9
    for _, enemy in ipairs(S.enemies) do
        local d = distance(ship, enemy.id)
        if d < near then best, near = enemy.id, d end
    end
    if best == 0 then return false end
    steer.set_weight(ship, "formation", 0)
    bt.set_value(ship, "target", best)
    return true
end

--- Behaviour tree leaf of a wingman: fire at its target.
---@param ship entity
---@return boolean fired
function M.wingman_fire(ship)
    local target = bt.get_value(ship, "target")
    if target == nil or target == 0 or distance(ship, target) > 120 then return false end
    local sx, sy, sz = transform.get_world_position(ship)
    local tx, ty, tz = transform.get_world_position(target)
    local lx, ly, lz = tx - sx, ty - sy, tz - sz
    local l = length(lx, ly, lz)
    shoot(ship, lx / l, ly / l, lz / l, "wing")
    return true
end

--- Behaviour tree leaf of a wingman: back in formation on the player.
---@param ship entity
---@return boolean done
function M.wingman_escort(ship)
    steer.set_weight(ship, "pursue", 0)
    steer.set_weight(ship, "formation", 1)
    return true
end

--- Hull of the player, in percent.
---@pure
---@return number hull
function M.hull()
    return S.hull
end

--- Hull of a freighter (1 to 3), from 0 to 1.
---@pure
---@param index integer
---@return number hull
function M.freighter_hull(index)
    local freighter = S.freighters[index]
    return freighter and freighter.hull / 100 or 0
end

--- Freighters still flying.
---@pure
---@return integer alive
function M.freighters_alive()
    local alive = 0
    for _, freighter in ipairs(S.freighters) do
        if freighter.hull > 0 then alive = alive + 1 end
    end
    return alive
end

--- How far the convoy has come along the route, from 0 to 1 (its last freighter still flying).
---@pure
---@return number progress
function M.progress()
    local last = nil
    for _, freighter in ipairs(S.freighters) do
        if freighter.hull > 0 then last = freighter end
    end
    if last == nil then return 0 end
    return math.min(1, spline.get_distance(last.id) / S.length)
end

--- What the convoy still has to fly to the gate, in metres (its last freighter still flying).
---@pure
---@return integer metres
function M.gate_distance()
    local last = nil
    for _, freighter in ipairs(S.freighters) do
        if freighter.hull > 0 then last = freighter end
    end
    if last == nil then return 0 end
    return math.max(0, math.floor(S.length - spline.get_distance(last.id) + 0.5))
end

--- Counters of the fight: enemies down, enemies in flight, waves come, shots fired and shots that hit, shots of the
--- enemies, hull of the convoy (the sum of its freighters).
---@pure
---@return integer kills
---@return integer enemies
---@return integer wave
---@return integer shots
---@return integer hits
---@return integer enemy_shots
---@return number convoy_hull
function M.counters()
    local convoy = 0
    for _, freighter in ipairs(S.freighters) do convoy = convoy + freighter.hull end
    return S.kills, #S.enemies, S.wave, S.shots, S.hits, S.enemy_shots, convoy
end

--- The order of the wingmen.
---@pure
---@return string order
function M.current_order()
    return S.order
end

return M
