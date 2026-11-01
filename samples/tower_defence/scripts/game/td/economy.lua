-- Tower prices, upgrades bought with the stars, the values read from the properties, the stars won.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local Eco, Fx = td.economy, td.effects

function Eco.unlocked_second_power()
    return settings.get(C.UNLOCK_SETTING, false) == true
end

function Eco.step_cost(kind, level)
    local base = S.stats[kind].cost
    if level == 2 then return math.floor(base * upgrade_2_share + 0.5) end
    if level == 3 then return math.floor(base * upgrade_3_share + 0.5) end
    return math.floor(base * power_share + 0.5)
end

function Eco.refund_of(tower)
    return math.floor(tower.spent * sell_share + 0.5)
end

function Eco.tier(family, level)
    return (S.bought[family] or 0) >= level
end

-- The values of the towers and the enemies, the tiers bought with the stars applied to the towers.
function Eco.read_stats()
    for _, family in ipairs(C.UPGRADE_FAMILIES) do
        S.bought[family] = settings.get("tower_up_" .. family, 0)
    end
    Fx.list, Fx.cache = {}, {}
    Fx.add_effects(time_effects, "time")
    for _, key in ipairs(modifiers) do
        local modifier = Fx.MODIFIERS[key]
        if modifier ~= nil then
            Fx.add_effects(modifier[3], "modifier")
        else
            log.warn("Tower defence: no modifier '" .. key .. "'.")
        end
    end
    for _, unlock in ipairs(Fx.UNLOCK_EFFECTS) do
        if settings.get(string.format("tower_done_%02d", unlock[1]), false) == true then
            Fx.add_effects(unlock[2], "unlock")
        end
    end
    Eco.read_waves()
    Eco.read_towers()
    Eco.read_enemies()
    local a, c, f = S.stats.archer, S.stats.cannon, S.stats.frost
    if Eco.tier("archer", 1) then a.damage = a.damage * 1.15 end
    if Eco.tier("archer", 2) then a.range = a.range * 1.12 end
    if Eco.tier("archer", 3) then a.reload = a.reload * 0.85 end
    if Eco.tier("cannon", 1) then c.cost = math.floor(c.cost * 0.85 + 0.5) end
    if Eco.tier("cannon", 2) then c.splash = c.splash * 1.2 end
    if Eco.tier("cannon", 3) then c.damage = c.damage * 1.2 end
    if Eco.tier("frost", 1) then f.time = f.time * 1.3 end
    if Eco.tier("frost", 2) then f.slow = f.slow * 0.85 end
    if Eco.tier("frost", 3) then f.range = f.range * 1.12 end
    if Eco.tier("archer", 4) then a.range = a.range * 1.1 end
    if Eco.tier("cannon", 4) then c.reload = c.reload * 0.85 end
    if Eco.tier("frost", 4) then f.damage = f.damage * 1.2 end
    for _, kind in ipairs(C.KINDS) do
        S.stats[kind].cost = math.floor(S.stats[kind].cost * Fx.factor_of(kind, true, "cost", "any") + 0.5)
    end
    local bounty = Eco.tier("treasury", 2) and 1.1 or 1.0
    for _, kind in ipairs(C.FOE_KINDS) do
        S.stats[kind].gold = math.floor(S.stats[kind].gold * bounty + 0.5)
    end
end

-- The waves of the level, a row of `waves_table` each: its groups ("kind count gap [road]" separated by ";") and
-- whether it is fought at night.
function Eco.read_waves()
    S.waves, Fx.nights = {}, {}
    for _, key in ipairs(datatable.get_keys(waves_table) or {}) do
        local row = datatable.get_row(waves_table, key)
        S.waves[#S.waves + 1] = row.groups or ""
        if row.night == true then Fx.nights[#S.waves] = true end
    end
    if #S.waves == 0 then log.warn("Tower defence: no wave in '" .. tostring(waves_table) .. "'.") end
end

-- The towers, a row of `towers_table` each: name, blurb, price and the values of a level 1 tower (a value only a
-- kind has is 0 for the others).
function Eco.read_towers()
    for _, kind in ipairs(C.KINDS) do
        local row = datatable.get_row(towers_table, kind) or {}
        local function only(value) return (value or 0) > 0 and value or nil end
        S.stats[kind] = { cost = row.cost or 0, range = row.range or 1, damage = row.damage or 0,
                          reload = row.reload or 1, splash = only(row.splash), slow = only(row.slow),
                          time = only(row.time) }
        C.NAMES[kind], C.BLURBS[kind] = row.name or kind, row.blurb or ""
    end
end

-- The Horde, a row of `enemies_table` each: name and how to beat it, health, speed, bounty, the share of an arrow it
-- takes, the lives it costs at the keep, its ability (`regen`, `ward`, `blast`, `rage`, `rally`) with its power,
-- radius and period, the height of its health bar and the size of its shadow.
function Eco.read_enemies()
    C.FOE_KINDS = datatable.get_keys(enemies_table) or {}
    for _, kind in ipairs(C.FOE_KINDS) do
        local row = datatable.get_row(enemies_table, kind)
        local stats = { hp = row.hp, speed = row.speed, gold = row.gold, arrow = row.arrow, lives = row.lives,
                        boss = row.boss == true, radius = row.radius, every = row.every }
        if row.ability ~= nil and row.ability ~= "none" then stats[row.ability] = row.power end
        S.stats[kind] = stats
        C.FOES[kind] = { row.name or kind, row.hint or "" }
        C.BAR_HEIGHT[kind], C.SHADOW_SIZE[kind] = row.bar_height or 0.8, row.shadow_size or 0.5
    end
end

function Eco.stars_for(kept)
    if kept >= 18 then return 3 end
    if kept >= 10 then return 2 end
    return 1
end

return Eco
