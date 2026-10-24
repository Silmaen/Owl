-- Day, night, level modifiers and unlocks as effects ("subject phase stat factor") and their texts.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local Fx = td.effects

Fx.list, Fx.cache, Fx.night, Fx.nights = {}, {}, false, {}

-- The level modifiers: a title, the text of the briefing and their effects ("subject phase stat factor").
Fx.MODIFIERS = {
    fog = { "Fog", "towers see less far", { "towers any range 0.85" } },
    blizzard = { "Blizzard", "frost bites harder, everyone moves slower",
                 { "frost any time 1.3", "horde any speed 0.88", "towers any reload 1.1" } },
    blood_moon = { "Blood moon", "the Horde has more life at night", { "horde night hp 1.3" } },
    drought = { "Drought", "cannons cost less, frost towers more", { "cannon any cost 0.8", "frost any cost 1.2" } },
    gale = { "Gale", "arrows drift, shells burst wider", { "archer any damage 0.85", "cannon any splash 1.25" } },
    plunder = { "Plunder", "the Horde carries more gold", { "horde any gold 1.25" } },
    long_night = { "Long night", "the night waves are darker still",
                   { "horde night speed 1.1", "towers night range 0.9" } },
}

-- The bonuses the campaign unlocks that are effects (`tower_done_NN` settings): level number, effect lines.
Fx.UNLOCK_EFFECTS = {
    { 4, { "archer any cost 0.9" } },
    { 6, { "towers any range 1.05" } },
    { 7, { "horde any gold 1.1" } },
    { 8, { "towers night range 1.1" } },
}

Fx.STAT_NAMES = { range = "range", damage = "damage", splash = "bursts", time = "chill", slow = "chill strength",
                     cost = "cost", speed = "speed", hp = "life", taken = "damage taken", ward = "wards", gold = "gold",
                     regen = "healing" }

function Fx.parse_effect(text)
    local subject, phase, stat, factor = string.match(text or "", "^%s*([%a_]+)%s+(%a+)%s+(%a+)%s+([%d%.]+)%s*$")
    if subject == nil then
        log.warn("Tower defence: effect '" .. tostring(text) .. "' is not 'subject phase stat factor'.")
        return nil
    end
    return { subject = subject, phase = phase, stat = stat, factor = tonumber(factor) }
end

function Fx.add_effects(lines, source)
    for _, line in ipairs(lines) do
        local effect = Fx.parse_effect(line)
        if effect ~= nil then
            effect.source = source
            Fx.list[#Fx.list + 1] = effect
        end
    end
end

function Fx.applies(effect, kind, tower, phase)
    local group = tower and "towers" or "horde"
    if effect.subject ~= kind and effect.subject ~= group then return false end
    if effect.phase == "any" then return true end
    return effect.phase == phase
end

-- The product of the factors of a stat for a kind, by the phase given (`any`: only the effects of every phase).
function Fx.factor_of(kind, tower, stat, phase)
    phase = phase or (Fx.night and "night" or "day")
    local key = kind .. "|" .. stat .. "|" .. phase
    local cached = Fx.cache[key]
    if cached ~= nil then return cached end
    local product = 1.0
    for _, effect in ipairs(Fx.list) do
        local wanted = phase ~= "any" or effect.phase == "any"
        if effect.stat == stat and wanted and Fx.applies(effect, kind, tower, phase) then
            product = product * effect.factor
        end
    end
    Fx.cache[key] = product
    return product
end

function Fx.percent(effect)
    local name = Fx.STAT_NAMES[effect.stat] or effect.stat
    if effect.stat == "reload" then
        local faster = math.floor((1.0 / effect.factor - 1.0) * 100 + 0.5)
        return effect.factor < 1.0 and string.format("fire %d%% faster", faster)
                or string.format("fire %d%% slower", math.floor((1.0 - 1.0 / effect.factor) * 100 + 0.5))
    end
    if effect.factor == 0.0 then return "no " .. name end
    local change = math.floor((effect.factor - 1.0) * 100 + (effect.factor >= 1.0 and 0.5 or -0.5))
    return string.format("%s%d%% %s", change >= 0 and "+" or "", change, name)
end

-- The day and night effects of a kind, as a line: "Day +10% damage. Night -20% range."
function Fx.time_text(kind)
    local parts = {}
    for _, phase in ipairs({ "day", "night" }) do
        local items = {}
        for _, effect in ipairs(Fx.list) do
            if effect.source == "time" and effect.subject == kind and effect.phase == phase then
                items[#items + 1] = Fx.percent(effect)
            end
        end
        if #items > 0 then parts[#parts + 1] = (phase == "day" and "Day " or "Night ") .. table.concat(items, ", ") end
    end
    return table.concat(parts, ". ")
end

return Fx
