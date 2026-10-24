-- The Horde: spawning, damage, wards, blasts, the walk along the road and the fall.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local UI, Cam, World, Fx, Vis, Foes = td.ui, td.camera, td.world, td.effects, td.visuals, td.enemies

function Foes.spawn_enemy(kind, road)
    local base = S.stats[kind]
    local x, y, z = World.along(0.0, road)
    local id = scene.spawn_prefab("prefabs/enemy_" .. kind .. ".owlprefab", Cam.billboard_at(x, y, z))
    if id == 0 then return end
    local _, size = transform.get_scale(id)
    local lift = size * C.FOOT_SHARE
    local bar = scene.spawn_prefab("prefabs/health_bar.owlprefab", x, y + 1.0, z)
    UI.show(bar, false)
    local shadow = scene.spawn_prefab("prefabs/shadow.owlprefab", x, y + 0.03, z)
    if shadow ~= 0 then
        transform.set_rotation(shadow, -math.pi / 2, 0.0, 0.0)
        transform.set_scale(shadow, C.SHADOW_SIZE[kind], C.SHADOW_SIZE[kind], 1.0)
    end
    local hp = base.hp * (1.0 + hp_growth * (S.wave - 1)) * Fx.factor_of(kind, false, "hp")
    S.spawned = S.spawned + 1
    if not S.foes_seen[kind] then
        S.foes_seen[kind] = true
        local foe = C.FOES[kind]
        if foe ~= nil and string.find("," .. new_enemies .. ",", "," .. kind .. ",", 1, true) then
            local times = Fx.time_text(kind)
            UI.message(foe[1] .. "!\n" .. foe[2] .. (times ~= "" and ("\n" .. times) or ""), 4.0)
        end
        gamestate.set("tower_seen_" .. kind, true)
    end
    if road == 2 then gamestate.set("tower_spawned_b", (gamestate.get("tower_spawned_b", 0)) + 1) end
    local side = ((S.spawned * 37) % 7) / 6.0 - 0.5
    S.enemies[#S.enemies + 1] = {
        id = id, bar = bar, shadow = shadow, kind = kind, hp = hp, max = hp, speed = base.speed, gold = base.gold,
        arrow = base.arrow, regen = base.regen or 0.0, road = road or 1, distance = 0.0, side = side * 0.28,
        slow = 0.0, slow_factor = 1.0, frozen = 0.0, frost_hits = 0, burn = 0.0, x = x, y = y, z = z, dx = 1.0,
        dz = 0.0, walk = S.spawned * 0.37, frame = -1, lift = lift, lives = base.lives or 1, boss = base.boss == true,
        ward = 0.0, ward_clock = base.ward and 1.5 or nil, ward_give = base.ward, blast = base.blast, rage = base.rage,
    }
end

function Foes.remove_enemy(index, dying)
    local foe = S.enemies[index]
    if foe.bar ~= 0 then scene.destroy_entity(foe.bar) end
    if foe.shadow ~= 0 then scene.destroy_entity(foe.shadow) end
    if dying then
        S.corpses[#S.corpses + 1] = { id = foe.id, direction = foe.frame % C.DIRECTIONS, t = 0.0 }
    else
        scene.destroy_entity(foe.id)
    end
    foe.dead = true
    table.remove(S.enemies, index)
end

-- Damage to an enemy: a ward takes it first unless it burns (`through_ward`); a kill pays and may blow a keg.
function Foes.hurt(foe, amount, through_ward)
    if foe.dead then return end
    if foe.vulnerable then amount = amount * aura_damage end
    amount = amount * Fx.factor_of(foe.kind, false, "taken")
    if foe.ward > 0.0 and not through_ward then
        local taken = math.min(foe.ward, amount)
        foe.ward = foe.ward - taken
        amount = amount - taken
    end
    foe.hp = foe.hp - amount
    if foe.hp <= 0 then
        local pay = math.floor(foe.gold * Fx.factor_of(foe.kind, false, "gold") + 0.5)
        S.gold = S.gold + pay
        S.earned = S.earned + pay
        S.kills = S.kills + 1
        UI.play("sounds/game/pop.wav")
        Vis.emit(foe.x, foe.y + 0.35, foe.z, 6, 1.4, 0.75, 0.15, 0.1, 0.12, 0.45, 1.2)
        for index, other in ipairs(S.enemies) do
            if other == foe then
                Foes.remove_enemy(index, true)
                break
            end
        end
        if foe.blast ~= nil then Foes.blast(foe) end
        return
    end
    foe.flash = 0.09
    if foe.bar ~= 0 then
        local share = foe.hp / foe.max
        UI.show(foe.bar, true)
        transform.set_scale(foe.bar, 0.5 * share, 0.07, 1.0)
        entity.set_tint(foe.bar, 1.0 - share * 0.8, 0.25 + share * 0.65, 0.2, 1.0)
    end
end

-- A sapper killed on the road: its keg bursts and hurts the Horde around it.
function Foes.blast(foe)
    UI.play("sounds/game/cannon.wav")
    Vis.spawn_ring(foe.x, foe.y, foe.z, sapper_blast_radius, 1.0, 0.6, 0.2)
    Vis.emit(foe.x, foe.y + 0.3, foe.z, 14, 2.2, 1.0, 0.55, 0.15, 0.14, 0.5, 1.4)
    local hit = {}
    for _, other in ipairs(S.enemies) do
        local dx, dz = other.x - foe.x, other.z - foe.z
        if dx * dx + dz * dz <= sapper_blast_radius * sapper_blast_radius then hit[#hit + 1] = other end
    end
    for _, other in ipairs(hit) do Foes.hurt(other, foe.blast) end
    gamestate.set("tower_blasts", gamestate.get("tower_blasts", 0) + 1)
end

-- An ogre mage wards the Horde around it (itself included) every few seconds.
function Foes.cast_ward(mage)
    for _, other in ipairs(S.enemies) do
        local dx, dz = other.x - mage.x, other.z - mage.z
        if dx * dx + dz * dz <= mage_ward_radius * mage_ward_radius then
            other.ward = math.max(other.ward, mage.ward_give * Fx.factor_of(mage.kind, false, "ward"))
        end
    end
    Vis.spawn_ring(mage.x, mage.y, mage.z, mage_ward_radius, 0.55, 0.75, 1.0)
    gamestate.set("tower_wards", gamestate.get("tower_wards", 0) + 1)
end

-- The speed factor of an enemy near the chieftain (its war cry).
function Foes.rally_of(foe)
    for _, other in ipairs(S.enemies) do
        if other.boss and other ~= foe then
            local dx, dz = other.x - foe.x, other.z - foe.z
            if dx * dx + dz * dz <= chieftain_rally_radius * chieftain_rally_radius then return chieftain_rally end
        end
    end
    return 1.0
end

-- The fallen: the three frames of the fall, then a fade.
function Foes.update_corpses(dt)
    local index = 1
    while index <= #S.corpses do
        local corpse = S.corpses[index]
        corpse.t = corpse.t + dt
        local row = C.DEATH_ROW + math.min(2, math.floor(corpse.t / 0.12))
        isometric.set_sprite_frame(corpse.id, row * C.DIRECTIONS + corpse.direction)
        if corpse.t > 0.45 then
            entity.set_tint(corpse.id, 1.0, 1.0, 1.0, math.max(0.0, 1.0 - (corpse.t - 0.45) / 0.5))
        end
        if corpse.t > 0.95 then
            scene.destroy_entity(corpse.id)
            table.remove(S.corpses, index)
        else
            index = index + 1
        end
    end
end

-- The column of a sheet for a heading on the ground plane: 0 toward the camera, then clockwise seen from above.
function Foes.direction_of(dx, dz)
    local right = dx * S.basis.right[1] + dz * S.basis.right[3]
    local toward = dx * S.basis.toward[1] + dz * S.basis.toward[2]
    local angle = math.atan(right, toward)
    return math.floor(angle / (2.0 * math.pi / C.DIRECTIONS) + 0.5) % C.DIRECTIONS
end

function Foes.update_enemies(dt)
    local index = 1
    while index <= #S.enemies do
        local foe = S.enemies[index]
        local speed = foe.speed * Foes.rally_of(foe) * Fx.factor_of(foe.kind, false, "speed")
        if foe.rage ~= nil then speed = speed * (1.0 + foe.rage * (1.0 - foe.hp / foe.max)) end
        if foe.ward_clock ~= nil then
            foe.ward_clock = foe.ward_clock - dt
            if foe.ward_clock <= 0.0 then
                foe.ward_clock = mage_ward_every
                Foes.cast_ward(foe)
            end
        end
        if foe.frozen > 0.0 then
            foe.frozen = foe.frozen - dt
            speed = 0.0
        elseif foe.slow > 0.0 then
            foe.slow = foe.slow - dt
            speed = speed * foe.slow_factor
        end
        if foe.burn > 0.0 then
            foe.burn = foe.burn - dt
            Foes.hurt(foe, fire_damage * dt, true)
        elseif foe.regen > 0.0 and foe.hp < foe.max then
            foe.hp = math.min(foe.max, foe.hp + foe.regen * Fx.factor_of(foe.kind, false, "regen") * dt)
        end
        if not foe.dead then
            local step = speed * dt
            foe.distance = foe.distance + step
            S.travel = S.travel + step
            local length = S.routes[foe.road].total
            if foe.distance >= length then
                -- At the gate of the keep: two blows, then the life is lost.
                foe.distance = length
                foe.attack = (foe.attack or 0.0) + dt
                local row = C.ATTACK_ROW + (math.floor(foe.attack / 0.18) % 2)
                isometric.set_sprite_frame(foe.id, row * C.DIRECTIONS + Foes.direction_of(foe.dx, foe.dz))
                if foe.attack >= 0.72 then
                    S.lives_left = S.lives_left - foe.lives
                    S.leaks = S.leaks + 1
                    UI.play("sounds/game/leak.wav")
                    Vis.emit(foe.x, foe.y + 0.5, foe.z, 5, 1.2, 0.9, 0.85, 0.7, 0.14, 0.4, 0.6)
                    UI.message("The keep is hit! " .. math.max(0, S.lives_left) .. " lives left", 1.5)
                    Foes.remove_enemy(index, false)
                else
                    index = index + 1
                end
            else
                local x, y, z, dx, dz = World.along(foe.distance, foe.road)
                -- Walk beside the middle of the road, on the right of the heading.
                x, z = x - dz * foe.side, z + dx * foe.side
                foe.x, foe.y, foe.z, foe.dx, foe.dz = x, y, z, dx, dz
                foe.walk = foe.walk + step * 3.2
                local frame = (math.floor(foe.walk) % 4) * C.DIRECTIONS + Foes.direction_of(dx, dz)
                if speed == 0.0 then frame = Foes.direction_of(dx, dz) end
                if frame ~= foe.frame then
                    foe.frame = frame
                    isometric.set_sprite_frame(foe.id, frame)
                end
                transform.set_position(foe.id, Cam.billboard_at(x, y, z, foe.lift))
                if foe.bar ~= 0 then
                    transform.set_position(foe.bar, Cam.billboard_at(x, y, z, C.BAR_HEIGHT[foe.kind]))
                end
                if foe.shadow ~= 0 then transform.set_position(foe.shadow, x, y + 0.03, z) end
                local tint = foe.frozen > 0.0 and "frozen" or (foe.slow > 0.0 and "slow" or (foe.burn > 0.0 and "burn"
                        or (foe.ward > 0.0 and "ward" or "plain")))
                if (foe.flash or 0.0) > 0.0 then
                    foe.flash = foe.flash - dt
                    tint = "flash"
                end
                if foe.tinted ~= tint then
                    foe.tinted = tint
                    if tint == "flash" then entity.set_tint(foe.id, 1.0, 0.4, 0.35, 1.0)
                    elseif tint == "frozen" then entity.set_tint(foe.id, 0.75, 0.95, 1.0, 1.0)
                    elseif tint == "slow" then entity.set_tint(foe.id, 0.65, 0.82, 1.0, 1.0)
                    elseif tint == "burn" then entity.set_tint(foe.id, 1.0, 0.7, 0.5, 1.0)
                    elseif tint == "ward" then entity.set_tint(foe.id, 0.8, 0.75, 1.0, 1.0)
                    else entity.set_tint(foe.id, 1.0, 1.0, 1.0, 1.0) end
                end
                index = index + 1
            end
        end
    end
end

return Foes
