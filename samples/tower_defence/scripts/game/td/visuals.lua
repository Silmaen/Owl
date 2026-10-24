-- Puffs, rings, fire and sparks.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local UI, Cam, Vis = td.ui, td.camera, td.visuals

function Vis.spawn_puff(x, y, z, r, g, b, size, duration)
    local id = scene.spawn_prefab("prefabs/puff.owlprefab", Cam.billboard_at(x, y, z, 0.2))
    if id == 0 then return end
    entity.set_tint(id, r, g, b, 0.85)
    S.bursts[#S.bursts + 1] = { id = id, kind = "puff", t = 0.0, duration = duration, size = size, r = r, g = g, b = b }
end

function Vis.spawn_ring(x, y, z, radius, r, g, b)
    local id = scene.spawn_prefab("prefabs/shock_ring.owlprefab", x, y + 0.05, z)
    if id == 0 then return end
    transform.set_rotation(id, -math.pi / 2, 0.0, 0.0)
    S.bursts[#S.bursts + 1] = { id = id, kind = "ring", t = 0.0, duration = 0.4, size = radius * 2.0, r = r, g = g,
                                b = b }
end

function Vis.spawn_fire(x, y, z, radius)
    local id = scene.spawn_prefab("prefabs/fire.owlprefab", Cam.billboard_at(x, y, z, 0.28))
    if id == 0 then return end
    S.bursts[#S.bursts + 1] = { id = id, kind = "fire", t = 0.0, duration = fire_time, x = x, y = y, z = z,
                              radius = radius }
end

-- Sparks: a pool of small glowing sprites for trails and impacts, reused rather than spawned each time.
function Vis.emit(x, y, z, count, speed, r, g, b, size, life, rise)
    for _ = 1, count do
        local spark = nil
        for _, candidate in ipairs(S.sparks) do
            if not candidate.active then
                spark = candidate
                break
            end
        end
        if spark == nil and #S.sparks < C.MAX_SPARKS then
            local id = scene.spawn_prefab("prefabs/spark.owlprefab", x, y, z)
            if id ~= 0 then
                spark = { id = id }
                S.sparks[#S.sparks + 1] = spark
            end
        end
        if spark == nil then return end
        local a, e = math.random() * 2.0 * math.pi, math.random() * 0.8 + 0.2
        spark.active, spark.t, spark.life, spark.size = true, 0.0, life * (0.7 + math.random() * 0.6), size
        spark.x, spark.y, spark.z = x, y, z
        spark.vx, spark.vy, spark.vz = math.cos(a) * speed * e, (rise or 0.0) + speed * (math.random() - 0.3),
                math.sin(a) * speed * e
        spark.r, spark.g, spark.b = r, g, b
        UI.show(spark.id, true)
        entity.set_tint(spark.id, r, g, b, 1.0)
    end
end

function Vis.update_sparks(dt)
    for _, spark in ipairs(S.sparks) do
        if spark.active then
            spark.t = spark.t + dt
            local t = spark.t / spark.life
            if t >= 1.0 then
                spark.active = false
                UI.show(spark.id, false)
            else
                spark.vy = spark.vy - 3.0 * dt
                spark.x, spark.y, spark.z = spark.x + spark.vx * dt, spark.y + spark.vy * dt, spark.z + spark.vz * dt
                transform.set_position(spark.id, spark.x, spark.y, spark.z)
                local size = spark.size * (1.0 - 0.6 * t)
                transform.set_scale(spark.id, size, size, 1.0)
                entity.set_tint(spark.id, spark.r, spark.g, spark.b, 1.0 - t)
            end
        end
    end
end

function Vis.update_effects(dt)
    local index = 1
    while index <= #S.bursts do
        local effect = S.bursts[index]
        effect.t = effect.t + dt
        local t = math.min(1.0, effect.t / effect.duration)
        if effect.kind == "puff" then
            local size = effect.size * (0.5 + 0.7 * t)
            transform.set_scale(effect.id, size, size, 1.0)
            entity.set_tint(effect.id, effect.r, effect.g, effect.b, 0.85 * (1.0 - t))
        elseif effect.kind == "ring" then
            local size = effect.size * (0.3 + 0.9 * t)
            transform.set_scale(effect.id, size, size, 1.0)
            entity.set_tint(effect.id, effect.r, effect.g, effect.b, 0.9 * (1.0 - t))
        elseif effect.kind == "fire" then
            for _, foe in ipairs(S.enemies) do
                local dx, dz = foe.x - effect.x, foe.z - effect.z
                if dx * dx + dz * dz <= effect.radius * effect.radius then foe.burn = 0.5 end
            end
            local fade = t > 0.75 and (1.0 - t) * 4.0 or 1.0
            entity.set_tint(effect.id, 1.0, 1.0, 1.0, fade)
        end
        if t >= 1.0 then
            scene.destroy_entity(effect.id)
            table.remove(S.bursts, index)
        else
            index = index + 1
        end
    end
end

return Vis
