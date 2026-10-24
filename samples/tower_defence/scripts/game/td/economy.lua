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
    Fx.nights = {}
    for number in string.gmatch(night_waves or "", "%d+") do Fx.nights[tonumber(number)] = true end
    S.stats.archer = { cost = archer_cost, range = archer_range, damage = archer_damage, reload = archer_reload }
    S.stats.cannon = { cost = cannon_cost, range = cannon_range, damage = cannon_damage, reload = cannon_reload,
                     splash = cannon_splash }
    S.stats.frost = { cost = frost_cost, range = frost_range, damage = frost_damage, reload = frost_reload,
                    slow = frost_slow, time = frost_time }
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
    S.stats.goblin = { hp = goblin_hp, speed = goblin_speed, gold = goblin_gold, arrow = 1.0 }
    S.stats.grunt = { hp = grunt_hp, speed = grunt_speed, gold = grunt_gold, arrow = 1.0 }
    S.stats.ogre = { hp = ogre_hp, speed = ogre_speed, gold = ogre_gold, arrow = ogre_arrow }
    S.stats.wolf = { hp = wolf_hp, speed = wolf_speed, gold = wolf_gold, arrow = 1.0 }
    S.stats.troll = { hp = troll_hp, speed = troll_speed, gold = troll_gold, arrow = 1.0, regen = troll_regen }
    S.stats.ogre_mage = { hp = mage_hp, speed = mage_speed, gold = mage_gold, arrow = 1.0, ward = mage_ward }
    S.stats.sapper = { hp = sapper_hp, speed = sapper_speed, gold = sapper_gold, arrow = 1.0, lives = sapper_lives,
                     blast = sapper_blast }
    S.stats.berserker = { hp = berserker_hp, speed = berserker_speed, gold = berserker_gold, arrow = 1.0,
                        rage = berserker_rage }
    S.stats.chieftain = { hp = chieftain_hp, speed = chieftain_speed, gold = chieftain_gold, arrow = chieftain_arrow,
                        lives = chieftain_lives, boss = true, scale = 1.35 }
    for _, kind in ipairs(C.KINDS) do
        S.stats[kind].cost = math.floor(S.stats[kind].cost * Fx.factor_of(kind, true, "cost", "any") + 0.5)
    end
    local bounty = Eco.tier("treasury", 2) and 1.1 or 1.0
    for _, kind in ipairs({ "goblin", "grunt", "ogre", "wolf", "troll", "ogre_mage", "sapper", "berserker",
                            "chieftain" }) do
        S.stats[kind].gold = math.floor(S.stats[kind].gold * bounty + 0.5)
    end
end

function Eco.stars_for(kept)
    if kept >= 18 then return 3 end
    if kept >= 10 then return 2 end
    return 1
end

return Eco
