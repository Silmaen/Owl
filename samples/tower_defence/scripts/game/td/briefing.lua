-- The briefing card of a level (a modal window, icons by line) and the legend of the pause window.

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

-- Frames of `textures/ui/briefing_icons.png` (6 x 3): the moon and the sun, the modifiers, then the Horde.
local CARD_HEIGHT, BODY_HEIGHT = 0.8, 0.68   -- the sizes of the card in the scene, with every line
local ICONS = { moon = 0, sun = 1, fog = 2, blizzard = 3, blood_moon = 4, drought = 5, gale = 6, plunder = 7,
                long_night = 8, goblin = 9, grunt = 10, ogre = 11, wolf = 12, troll = 13, ogre_mage = 14, sapper = 15,
                berserker = 16, chieftain = 17 }

-- The lines of the briefing: an icon and a text each.
function Brief.lines()
    local out = {}
    local nights = Brief.night_list()
    if #nights > 0 then
        out[#out + 1] = { ICONS.moon, S.wave_count .. " waves, the night ones: " .. table.concat(nights, ", ") .. "." }
    else
        out[#out + 1] = { ICONS.sun, S.wave_count .. " waves, all by day." }
    end
    for _, key in ipairs(modifiers) do
        local modifier = Fx.MODIFIERS[key]
        if modifier ~= nil then out[#out + 1] = { ICONS[key] or ICONS.sun, modifier[1] .. ": " .. modifier[2] .. "." } end
    end
    for kind in string.gmatch(new_enemies, "[%a_]+") do
        local foe = C.FOES[kind]
        if foe ~= nil then out[#out + 1] = { ICONS[kind] or ICONS.sun, "New: " .. foe[1] .. ", " .. foe[2] .. "." } end
    end
    return out
end

-- The briefing card, a modal window: the level, its nights, its modifiers and the enemies it brings first, a row each.
function Brief.open_briefing()
    if S.hud.briefing == 0 then return end
    local lines = Brief.lines()
    for index, row in ipairs(S.hud.briefing_rows) do
        local line = lines[index]
        UI.show(row.row, line ~= nil)
        if line ~= nil then
            ui.set_image_frame(row.icon, line[1])
            ui.set_text(row.text, line[2])
        end
    end
    if S.hud.briefing_title ~= 0 then ui.set_text(S.hud.briefing_title, level_name) end
    -- One button is centred alone in the row; with Skip tutorial both share it.
    local offered = td.tutorial.offered()
    UI.show(S.hud.briefing_skip, offered)
    ui.set_size(S.hud.briefing_buttons, offered and 0.9 or 0.42, 0.08)
    ui.set_size(S.hud.briefing_button, offered and 0.4 or 0.9, 0.9)
    Brief.size = { card = CARD_HEIGHT, body = BODY_HEIGHT }
    Brief.open = true
    time.set_paused(true)
    UI.show(S.hud.briefing, true)
    ui.raise_window(S.hud.briefing)
    if S.hud.briefing_button ~= 0 then ui.set_focus(S.hud.briefing_button) end
    gamestate.set("tower_briefing", true)
    gamestate.set("tower_briefing_lines", #lines)
    gamestate.set("tower_tutorial_offered", td.tutorial.offered())
end

-- The card fits its lines: once laid out, its height and the height of its body follow the bottom of the buttons,
-- whatever the resolution and the interface size (the sizes are in screen heights, the layout in pixels).
function Brief.fit()
    if not Brief.open or Brief.size == nil then return end
    local _, card_y, _, card_h = ui.get_rect(S.hud.briefing)
    local _, body_y, _, body_h = ui.get_rect(S.hud.briefing_body)
    local _, row_y, _, row_h = ui.get_rect(S.hud.briefing_buttons)
    if card_y == nil or body_y == nil or row_y == nil or card_h <= 0 or body_h <= 0 then return end
    local _, height = scene.get_viewport_size()
    local margin = 16 * (height or 720) / 720
    local bottom = row_y + row_h
    local wanted_body = bottom - body_y + 2
    local wanted_card = bottom - card_y + margin
    if math.abs(wanted_card - card_h) <= 1.0 and math.abs(wanted_body - body_h) <= 1.0 then
        gamestate.set("tower_briefing_fitted", true)
        return
    end
    Brief.size.body = Brief.size.body * wanted_body / body_h
    Brief.size.card = Brief.size.card * wanted_card / card_h
    ui.set_size(S.hud.briefing_body, 0.9, Brief.size.body)
    ui.set_size(S.hud.briefing, 1.16, Brief.size.card)
end

function Brief.close_briefing()
    if not Brief.open then return end
    Brief.open = false
    time.set_paused(false)
    UI.show(S.hud.briefing, false)
    gamestate.set("tower_briefing", false)
    gamestate.set("tower_briefing_seen", true)
    td.tutorial.start()
end

return Brief
