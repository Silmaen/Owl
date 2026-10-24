-- HUD texts and messages, sounds, and the game state mirrored for the scenarios.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local UI, Fx, Wheel = td.ui, td.effects, td.wheel

function UI.set_text(key, text)
    if S.hud[key] ~= nil and S.hud[key] ~= 0 and S.shown[key] ~= text then
        ui.set_text(S.hud[key], text)
        S.shown[key] = text
    end
end

function UI.show(id, visible)
    if id ~= nil and id ~= 0 then ui.set_visible(id, visible) end
end

function UI.message(text, seconds)
    UI.show(S.hud.briefing, false)
    UI.set_text("message", text)
    UI.show(S.hud.message, true)
    S.message_clock = seconds or 2.0
end

function UI.play(path)
    sound.play(path)
end

function UI.mirror()
    gamestate.set("tower_gold", S.gold)
    gamestate.set("tower_lives", math.max(0, S.lives_left))
    gamestate.set("tower_wave", S.wave)
    gamestate.set("tower_waves", S.wave_count)
    gamestate.set("tower_countdown", math.ceil(S.countdown))
    gamestate.set("tower_towers", #S.towers)
    gamestate.set("tower_kills", S.kills)
    gamestate.set("tower_leaks", S.leaks)
    gamestate.set("tower_spawned", S.spawned)
    gamestate.set("tower_alive", #S.enemies)
    gamestate.set("tower_ring", S.ring == nil and 0 or (S.ring.pad.tower ~= nil and 2 or 1))
    gamestate.set("tower_hover", S.hover)
    gamestate.set("tower_paused", S.paused)
    gamestate.set("tower_speed", time.get_scale())
    gamestate.set("tower_travel", math.floor(S.travel * 100) / 100)
    gamestate.set("tower_paused_travel", S.paused_travel)
    gamestate.set("tower_bonus", S.bonus_total)
    gamestate.set("tower_cursor", S.cursor_pad and S.cursor_pad.index or 0)
    gamestate.set("tower_yaw", math.floor(S.view.yaw_goal * 180 / math.pi + 0.5))
    gamestate.set("tower_hours", math.floor((S.hours % 24.0) * 10) / 10)
    gamestate.set("tower_night", S.night_lit == true)
    gamestate.set("tower_upgrades", S.upgrades)
    local top, power = 0, ""
    for _, tower in ipairs(S.towers) do
        if tower.level > top then top = tower.level end
        if tower.power ~= nil then power = tower.power end
    end
    gamestate.set("tower_top_level", top)
    gamestate.set("tower_power", power)
    local first = S.enemies[1]
    gamestate.set("tower_enemy_frame", first and first.frame or -1)
    gamestate.set("tower_enemy_direction", first and first.frame % C.DIRECTIONS or -1)
end

-- The wave the phase changes at, after the current one (nil when it never changes again).
function UI.next_change()
    for number = S.wave + 1, S.wave_count do
        if (Fx.nights[number] == true) ~= Fx.night then return number end
    end
    return nil
end

function UI.update_hud()
    local change = UI.next_change()
    local phase = Fx.night and "Night" or "Day"
    UI.set_text("phase", change and string.format("%s - %s at wave %d", phase, Fx.night and "dawn" or "night", change)
            or (phase .. " to the end"))
    if S.hud.phase_icon ~= 0 and S.shown.phase_icon ~= Fx.night then
        ui.set_image_frame(S.hud.phase_icon, Fx.night and 1 or 0)
        S.shown.phase_icon = Fx.night
    end
    UI.set_text("gold", "Gold " .. S.gold)
    UI.set_text("lives", "Lives " .. math.max(0, S.lives_left))
    if S.spawning ~= nil then
        UI.set_text("wave", "Wave " .. S.wave .. " / " .. S.wave_count .. " - incoming")
    elseif S.wave < S.wave_count then
        local next_in = " - next in " .. math.ceil(S.countdown) .. " s"
        UI.set_text("wave", "Wave " .. S.wave .. " / " .. S.wave_count .. next_in)
    else
        UI.set_text("wave", "Wave " .. S.wave .. " / " .. S.wave_count .. " - last one")
    end
    local can_call = S.spawning == nil and S.wave < S.wave_count and S.route ~= nil and S.ending == nil
    if S.hud.call ~= 0 then
        ui.set_button_enabled(S.hud.call, can_call)
        UI.set_text("call", can_call and ("Call wave +" .. math.floor(S.countdown * early_bonus)) or "Call wave")
    end
    UI.set_text("speed", S.fast and "Speed x2" or "Speed x1")
    if S.ring ~= nil then
        Wheel.refresh_badges()
        Wheel.place_wheel()
    end
end

return UI
