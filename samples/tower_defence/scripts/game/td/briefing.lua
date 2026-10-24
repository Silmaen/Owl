-- The briefing card of a level and the legend of the pause window.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local UI, Fx, Brief = td.ui, td.effects, td.briefing

Brief.open = false

-- The kinds of the Horde the waves of this level send, in order of first appearance.
function Brief.level_kinds()
    local seen, out = {}, {}
    for _, text in ipairs(waves) do
        for kind in string.gmatch(text, "([%a_]+)%s+%d") do
            if not seen[kind] and S.stats[kind] ~= nil then
                seen[kind] = true
                out[#out + 1] = kind
            end
        end
    end
    return out
end

function Brief.modifier_lines()
    local out = {}
    for _, key in ipairs(modifiers) do
        local modifier = Fx.MODIFIERS[key]
        if modifier ~= nil then out[#out + 1] = modifier[1] .. ": " .. modifier[2] .. "." end
    end
    return out
end

-- The legend of the pause window: the day and night effects of the towers and of the Horde of this level, then the
-- modifiers of the level.
function Brief.effects_legend()
    local lines = { (Fx.night and "Now night." or "Now day.") .. " Day and night change the fight:" }
    for _, kind in ipairs(C.KINDS) do lines[#lines + 1] = C.NAMES[kind] .. ": " .. Fx.time_text(kind) end
    for _, kind in ipairs(Brief.level_kinds()) do
        local times = Fx.time_text(kind)
        if times ~= "" then lines[#lines + 1] = (C.FOES[kind] and C.FOES[kind][1] or kind) .. ": " .. times end
    end
    for _, line in ipairs(Brief.modifier_lines()) do lines[#lines + 1] = line end
    return table.concat(lines, "\n")
end

function Brief.night_list()
    local out = {}
    for number = 1, S.wave_count do
        if Fx.nights[number] then out[#out + 1] = tostring(number) end
    end
    return out
end

-- The briefing card: the level, its nights, its modifiers and the enemies it brings first.
function Brief.open_briefing()
    if S.hud.briefing == 0 then return end
    local lines = {}
    local nights = Brief.night_list()
    local when = #nights > 0 and (", the night ones: " .. table.concat(nights, ", ")) or ", all by day"
    lines[#lines + 1] = S.wave_count .. " waves" .. when .. "."
    for _, line in ipairs(Brief.modifier_lines()) do lines[#lines + 1] = line end
    for kind in string.gmatch(new_enemies, "[%a_]+") do
        local foe = C.FOES[kind]
        if foe ~= nil then lines[#lines + 1] = "New: " .. foe[1] .. ", " .. foe[2] .. "." end
    end
    if S.hud.briefing_title ~= 0 then ui.set_text(S.hud.briefing_title, level_name) end
    if S.hud.briefing_text ~= 0 then ui.set_text(S.hud.briefing_text, table.concat(lines, "\n")) end
    Brief.open = true
    time.set_paused(true)
    -- The card lies in the message line: shown with it, an empty message under it.
    UI.set_text("message", "")
    UI.show(S.hud.message, true)
    UI.show(S.hud.briefing, true)
    if S.hud.briefing_button ~= 0 then ui.set_focus(S.hud.briefing_button) end
    gamestate.set("tower_briefing", true)
end

function Brief.close_briefing()
    if not Brief.open then return end
    Brief.open = false
    time.set_paused(false)
    UI.show(S.hud.briefing, false)
    UI.show(S.hud.message, false)
    gamestate.set("tower_briefing", false)
    gamestate.set("tower_briefing_seen", true)
end

return Brief
