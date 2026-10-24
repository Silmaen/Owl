-- Towers: their stats, aim and shots, projectiles, building, raising and selling.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local UI, Eco, Cam, World = td.ui, td.economy, td.camera, td.world
local Fx, Vis, Foes, Towers, Wheel = td.effects, td.visuals, td.enemies, td.towers, td.wheel

-- The stats of a tower at its level and power-up, its range grown by the height of its pad.
function Towers.tower_stats(tower)
    local base = S.stats[tower.kind]
    local s = { range = base.range, damage = base.damage, reload = base.reload, splash = base.splash,
                slow = base.slow, time = base.time }
    if tower.level >= 2 then
        if tower.kind == "archer" then s.range, s.damage = s.range * 1.15, s.damage * 1.5
        elseif tower.kind == "cannon" then s.damage = s.damage * 1.5
        else s.slow, s.damage = s.slow * 0.8, s.damage * 1.5 end
    end
    if tower.level >= 3 then
        if tower.kind == "archer" then s.reload, s.damage = s.reload * 0.65, s.damage * 1.2
        elseif tower.kind == "cannon" then s.splash, s.damage = s.splash * 1.35, s.damage * 1.2
        else s.time, s.damage = s.time * 1.45, s.damage * 1.3 end
    end
    local kind = tower.kind
    s.range = s.range * Fx.factor_of(kind, true, "range")
    s.damage = s.damage * Fx.factor_of(kind, true, "damage")
    s.reload = s.reload * Fx.factor_of(kind, true, "reload")
    if s.splash ~= nil then s.splash = s.splash * Fx.factor_of(kind, true, "splash") end
    if s.time ~= nil then s.time = s.time * Fx.factor_of(kind, true, "time") end
    if s.slow ~= nil then s.slow = 1.0 - (1.0 - s.slow) * Fx.factor_of(kind, true, "slow") end
    s.range = s.range * (1.0 + height_bonus * tower.pad.level)
    return s
end

function Towers.in_range(tower, foe, range)
    local dx, dz = foe.x - tower.x, foe.z - tower.z
    return dx * dx + dz * dz <= range * range
end

-- The enemies in range, the one nearest the keep first.
function Towers.targets_of(tower, count)
    local range = tower.stats.range
    local found = {}
    for _, foe in ipairs(S.enemies) do
        if Towers.in_range(tower, foe, range) then found[#found + 1] = foe end
    end
    table.sort(found, function(a, b) return a.distance > b.distance end)
    local out = {}
    for i = 1, math.min(count, #found) do out[i] = found[i] end
    return out
end

function Towers.fire(tower, foe)
    local kind = tower.kind
    local key = (kind == "cannon" and tower.power == "fire") and "fire" or kind
    local sy = tower.y + C.SHOT_HEIGHT[tower.level]
    local id = scene.spawn_prefab(C.PROJECTILE[key], tower.x, sy, tower.z)
    if id == 0 then return end
    local shot = { id = id, kind = kind, power = tower.power, tower = tower, x = tower.x, y = sy, z = tower.z,
                   target = foe, t = 0.0, sx = tower.x, sy = sy, sz = tower.z, tx = foe.x, ty = foe.y, tz = foe.z }
    if kind == "cannon" then
        -- Aim where the enemy will be when the shell lands.
        local factor = foe.frozen > 0.0 and 0.0 or (foe.slow > 0.0 and foe.slow_factor or 1.0)
        local lead = foe.distance + foe.speed * factor * ball_flight
        shot.tx, shot.ty, shot.tz = World.along(math.min(lead, S.routes[foe.road].total), foe.road)
    end
    S.projectiles[#S.projectiles + 1] = shot
    if S.clock - (S.last_shot_sound[kind] or -1.0) > 0.12 then
        S.last_shot_sound[kind] = S.clock
        UI.play(C.SHOT_SOUND[kind])
    end
end

function Towers.update_towers(dt)
    for _, foe in ipairs(S.enemies) do foe.vulnerable = false end
    for _, tower in ipairs(S.towers) do
        if tower.power == "aura" then
            for _, foe in ipairs(S.enemies) do
                if Towers.in_range(tower, foe, tower.stats.range) then foe.vulnerable = true end
            end
        end
    end
    for _, tower in ipairs(S.towers) do
        Towers.aim(tower, dt)
        tower.cooldown = tower.cooldown - dt
        if tower.cooldown <= 0.0 then
            local found = Towers.targets_of(tower, tower.power == "double" and 2 or 1)
            if #found > 0 then
                for _, foe in ipairs(found) do Towers.fire(tower, foe) end
                tower.aim_x, tower.aim_z = found[1].x, found[1].z
                tower.kick = 0.15
                tower.cooldown = tower.stats.reload
            else
                tower.cooldown = 0.0
            end
        end
    end
end

function Towers.land(shot)
    local s = shot.tower.stats
    if shot.kind == "cannon" then
        UI.play("sounds/game/boom.wav")
        Vis.spawn_puff(shot.tx, shot.ty, shot.tz, 1.0, 0.7, 0.35, 1.6, 0.4)
        Vis.emit(shot.tx, shot.ty + 0.1, shot.tz, 10, 2.2, 0.45, 0.36, 0.26, 0.11, 0.6, 1.5)
        Vis.emit(shot.tx, shot.ty + 0.15, shot.tz, 6, 1.6, 1.0, 0.7, 0.25, 0.1, 0.35, 1.0)
        local radius = s.splash
        local hit = {}
        for _, foe in ipairs(S.enemies) do
            local dx, dz = foe.x - shot.tx, foe.z - shot.tz
            if dx * dx + dz * dz <= radius * radius then hit[#hit + 1] = foe end
        end
        for _, foe in ipairs(hit) do
            if shot.power == "shock" then foe.distance = math.max(0.0, foe.distance - shock_push) end
            Foes.hurt(foe, s.damage)
        end
        if shot.power == "fire" then Vis.spawn_fire(shot.tx, shot.ty, shot.tz, radius * 0.8) end
        if shot.power == "shock" then Vis.spawn_ring(shot.tx, shot.ty, shot.tz, radius, 1.0, 0.85, 0.6) end
        return
    end
    local foe = shot.target
    if foe == nil or foe.dead then return end
    if shot.kind == "archer" then
        UI.play("sounds/game/hit.wav")
        Vis.emit(foe.x, foe.y + 0.35, foe.z, 3, 1.0, 0.95, 0.85, 0.6, 0.07, 0.25, 0.6)
        if shot.power == "pierce" then
            local behind = {}
            for _, other in ipairs(S.enemies) do
                if other ~= foe and other.distance < foe.distance and other.distance > foe.distance - 1.4 then
                    behind[#behind + 1] = other
                end
            end
            table.sort(behind, function(a, b) return a.distance > b.distance end)
            for i = 1, math.min(2, #behind) do Foes.hurt(behind[i], s.damage * behind[i].arrow) end
        end
        Foes.hurt(foe, s.damage * foe.arrow)
    else
        foe.slow = s.time
        foe.slow_factor = foe.boss and (1.0 - (1.0 - s.slow) * 0.5) or s.slow
        if shot.power == "freeze" and not foe.boss then
            foe.frost_hits = foe.frost_hits + 1
            if foe.frost_hits % 3 == 0 then foe.frozen = freeze_time end
        end
        Vis.spawn_puff(foe.x, foe.y + 0.3, foe.z, 0.6, 0.85, 1.0, 0.7, 0.3)
        Vis.emit(foe.x, foe.y + 0.35, foe.z, 5, 1.2, 0.75, 0.92, 1.0, 0.08, 0.4, 0.4)
        Foes.hurt(foe, s.damage)
    end
end

function Towers.update_projectiles(dt)
    local index = 1
    while index <= #S.projectiles do
        local shot = S.projectiles[index]
        local arrived = false
        if shot.kind == "cannon" then
            shot.t = shot.t + dt / ball_flight
            local t = math.min(1.0, shot.t)
            shot.x = shot.sx + (shot.tx - shot.sx) * t
            shot.z = shot.sz + (shot.tz - shot.sz) * t
            shot.y = shot.sy + (shot.ty + 0.2 - shot.sy) * t + 2.5 * t * (1.0 - t)
            arrived = t >= 1.0
        else
            if shot.target ~= nil and not shot.target.dead then
                shot.tx, shot.ty, shot.tz = shot.target.x, shot.target.y + 0.3, shot.target.z
            end
            local dx, dy, dz = shot.tx - shot.x, shot.ty - shot.y, shot.tz - shot.z
            local distance = math.sqrt(dx * dx + dy * dy + dz * dz)
            local step = arrow_speed * dt
            if distance <= step or distance < 0.05 then
                arrived = true
            else
                shot.x, shot.y, shot.z = shot.x + dx / distance * step, shot.y + dy / distance * step,
                        shot.z + dz / distance * step
                if shot.kind == "archer" then transform.set_rotation(shot.id, 0.0, 0.0, Cam.screen_roll(dx, dy, dz)) end
            end
        end
        shot.trail = (shot.trail or 0.0) - dt
        if shot.trail <= 0.0 and not arrived then
            shot.trail = 0.035
            if shot.power == "fire" then
                Vis.emit(shot.x, shot.y, shot.z, 1, 0.25, 1.0, 0.55, 0.15, 0.16, 0.35, 0.3)
            elseif shot.kind == "frost" then
                Vis.emit(shot.x, shot.y, shot.z, 1, 0.15, 0.7, 0.9, 1.0, 0.09, 0.3, 0.0)
            elseif shot.kind == "cannon" then
                Vis.emit(shot.x, shot.y, shot.z, 1, 0.1, 0.55, 0.55, 0.55, 0.12, 0.4, 0.2)
            else
                Vis.emit(shot.x, shot.y, shot.z, 1, 0.05, 0.95, 0.92, 0.8, 0.05, 0.18, 0.0)
            end
        end
        if arrived then
            Towers.land(shot)
            scene.destroy_entity(shot.id)
            table.remove(S.projectiles, index)
        else
            transform.set_position(shot.id, shot.x, shot.y, shot.z)
            index = index + 1
        end
    end
end

function Towers.tower_prefab(tower)
    local key = tower.power or tostring(tower.level)
    return "prefabs/tower_" .. tower.kind .. "_" .. key .. ".owlprefab"
end

-- Spawn the prefab of a tower at its level and keep its weapons (the meshes tagged `TowerWeapon`), which turn to
-- their target and recoil at each shot.
function Towers.place_tower(tower)
    if tower.id ~= nil and tower.id ~= 0 then scene.destroy_entity(tower.id) end
    local before = {}
    for _, id in ipairs(scene.find_entities("TowerWeapon")) do before[id] = true end
    tower.id = scene.spawn_prefab(Towers.tower_prefab(tower), tower.x, tower.y + C.PAD_TOP, tower.z)
    tower.stats = Towers.tower_stats(tower)
    tower.weapons = {}
    for _, id in ipairs(scene.find_entities("TowerWeapon")) do
        if not before[id] then
            local x, y, z = transform.get_position(id)
            local sx = transform.get_scale(id)
            tower.weapons[#tower.weapons + 1] = { id = id, x = x, y = y, z = z, scale = sx }
        end
    end
    tower.kick = 0.0
end

-- Turn the weapons of a tower toward its target, recoiling after a shot (`tower.kick`); the crystals pulse instead.
function Towers.aim(tower, dt)
    tower.kick = math.max(0.0, (tower.kick or 0.0) - dt)
    local push = tower.kick / 0.15
    if tower.aim_x ~= nil then
        tower.yaw = math.atan(tower.aim_x - tower.x, tower.aim_z - tower.z) + weapon_yaw
    end
    local yaw = tower.yaw or 0.0
    for _, weapon in ipairs(tower.weapons or {}) do
        if tower.kind == "frost" then
            local size = weapon.scale * (1.0 + 0.25 * push)
            transform.set_scale(weapon.id, size, size, size)
        else
            transform.set_rotation(weapon.id, 0.0, yaw, 0.0)
            local back = 0.12 * push
            local x, z = weapon.x - math.sin(yaw) * back, weapon.z - math.cos(yaw) * back
            transform.set_position(weapon.id, x, weapon.y, z)
        end
    end
end

function Towers.build(pad, kind)
    if pad.tower ~= nil then return end
    local price = S.stats[kind].cost
    if S.gold < price then
        UI.message("Not enough gold", 1.0)
        return
    end
    S.gold = S.gold - price
    local tower = { kind = kind, level = 1, pad = pad, x = pad.x, y = pad.y, z = pad.z, cooldown = 0.2, spent = price }
    Towers.place_tower(tower)
    if tower.id == 0 then
        S.gold = S.gold + price
        return
    end
    pad.tower = tower
    S.towers[#S.towers + 1] = tower
    S.built = S.built + 1
    UI.play("sounds/game/build.wav")
    Wheel.close_ring()
end

function Towers.improve(pad, choice)
    local tower = pad.tower
    if tower == nil then return end
    if choice.locked then
        UI.message("Unlocked by winning " .. C.UNLOCK_NAME, 1.4)
        return
    end
    local price = Wheel.choice_price(pad, choice)
    if S.gold < price then
        UI.message("Not enough gold", 1.0)
        return
    end
    S.gold = S.gold - price
    tower.spent = tower.spent + price
    if choice.kind == "upgrade" then
        tower.level = tower.level + 1
    else
        tower.power = choice.kind
    end
    Towers.place_tower(tower)
    S.upgrades = S.upgrades + 1
    UI.play("sounds/game/build.wav")
    Wheel.close_ring()
end

function Towers.sell(pad)
    local tower = pad.tower
    if tower == nil then return end
    S.gold = S.gold + Eco.refund_of(tower)
    scene.destroy_entity(tower.id)
    for index, other in ipairs(S.towers) do
        if other == tower then
            table.remove(S.towers, index)
            break
        end
    end
    pad.tower = nil
    UI.play("sounds/game/sell.wav")
    Wheel.close_ring()
end

-- The towers standing when the level starts (`start_towers`: "pad kind level [power]", pads counted from 1).
function Towers.raise_start_towers()
    for _, text in ipairs(start_towers or {}) do
        local index, kind, level, power = string.match(text, "^%s*(%d+)%s+(%a+)%s+(%d)%s*(%a*)%s*$")
        local pad = S.pads[tonumber(index or 0)]
        if pad ~= nil and pad.tower == nil and S.stats[kind] ~= nil and S.stats[kind].cost ~= nil then
            local tower = { kind = kind, level = math.max(1, math.min(3, tonumber(level))), pad = pad, x = pad.x,
                            y = pad.y, z = pad.z, cooldown = 0.2, spent = S.stats[kind].cost }
            if power ~= "" and tower.level == 3 then tower.power = power end
            Towers.place_tower(tower)
            pad.tower = tower
            S.towers[#S.towers + 1] = tower
        else
            log.warn("Tower defence: start tower '" .. text .. "' is not 'pad kind level [power]' on a free pad.")
        end
    end
end

return Towers
