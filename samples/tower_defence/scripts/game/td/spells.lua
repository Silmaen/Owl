-- Rain of arrows and shield wall.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local UI, Vis, Foes, Spells = td.ui, td.visuals, td.enemies, td.spells

-- The enemy nearest the keep: the spells fall on it and around it.
function Spells.front_enemy()
    local best, share = nil, -1.0
    for _, foe in ipairs(S.enemies) do
        local progress = foe.distance / math.max(0.01, S.routes[foe.road].total)
        if progress > share then best, share = foe, progress end
    end
    return best
end

function Spells.cast(name)
    local spell = S.spells[name]
    if spell == nil or not spell.unlocked or spell.clock > 0.0 or S.ending ~= nil or S.paused then return end
    local target = Spells.front_enemy()
    if target == nil then
        UI.message("No enemy to strike", 1.0)
        return
    end
    spell.clock = spell.cooldown
    spell.casts = spell.casts + 1
    local hit = {}
    for _, foe in ipairs(S.enemies) do
        local dx, dz = foe.x - target.x, foe.z - target.z
        if dx * dx + dz * dz <= spell.radius * spell.radius then hit[#hit + 1] = foe end
    end
    if name == "rain" then
        UI.play("sounds/game/arrow.wav")
        for k = 1, 14 do
            local a, r = k * 2.4, (k % 5) * spell.radius * 0.2
            Vis.emit(target.x + math.cos(a) * r, target.y + 2.2, target.z + math.sin(a) * r, 1, 0.2, 0.95, 0.9, 0.75,
                    0.1, 0.5, -4.0)
        end
        Vis.spawn_ring(target.x, target.y, target.z, spell.radius, 0.95, 0.9, 0.7)
        for _, foe in ipairs(hit) do Foes.hurt(foe, spell.damage) end
    else
        UI.play("sounds/game/build.wav")
        Vis.spawn_ring(target.x, target.y, target.z, spell.radius, 1.0, 0.85, 0.3)
        Vis.emit(target.x, target.y + 0.4, target.z, 12, 1.6, 1.0, 0.85, 0.35, 0.12, 0.6, 0.8)
        for _, foe in ipairs(hit) do foe.frozen = math.max(foe.frozen, foe.boss and spell.time * 0.5 or spell.time) end
    end
    gamestate.set("tower_" .. name .. "_casts", spell.casts)
end

function Spells.update_spells(dt)
    for name, spell in pairs(S.spells) do
        spell.clock = math.max(0.0, spell.clock - dt)
        local id = S.hud[name]
        if id ~= 0 and spell.unlocked then
            ui.set_button_enabled(id, spell.clock <= 0.0 and #S.enemies > 0 and S.ending == nil)
            local waiting = spell.clock > 0.0 and string.format("%s %d s", spell.title, math.ceil(spell.clock))
            UI.set_text(name .. "_label", waiting or spell.title)
        end
    end
end

return Spells
