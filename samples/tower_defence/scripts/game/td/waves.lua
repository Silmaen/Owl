-- The waves, the day and night cycle and the sky.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local UI, Waves, Fx, Foes, Towers = td.ui, td.waves, td.effects, td.enemies, td.towers

-- The effects of the phase in the game state, for the scenarios: the range of a level 1 archer on the plain and the
-- speed factor of a goblin, in hundredths.
function Waves.mirror_effects()
    local archer = Towers.tower_stats({ kind = "archer", level = 1, pad = { level = 0 } })
    gamestate.set("tower_archer_range", math.floor(archer.range * 100 + 0.5))
    gamestate.set("tower_goblin_speed", math.floor(Fx.factor_of("goblin", false, "speed") * 100 + 0.5))
end

function Waves.sky_is_night(h)
    local clock_hour = h % 24.0
    return clock_hour >= 19.0 or clock_hour < 6.0
end

function Waves.update_sky(dt)
    if S.sky_id == 0 then return end
    if math.abs(S.hours_goal - S.hours) > 0.001 then
        local step = math.max(0.25, math.abs(S.hours_goal - S.hours)) * dt * 0.35
        if S.hours < S.hours_goal then
            S.hours = math.min(S.hours_goal, S.hours + step)
        else
            S.hours = math.max(S.hours_goal, S.hours - step)
        end
        entity.set_time_of_day(S.sky_id, S.hours % 24.0)
    end
    local night = Waves.sky_is_night(S.hours)
    if night ~= S.night_lit then
        S.night_lit = night
        for _, torch in ipairs(S.torch_ids) do
            entity.set_light_enabled(torch.light, night)
            if torch.glow ~= 0 then entity.set_light_enabled(torch.glow, night) end
            UI.show(torch.flame, night)
        end
    end
end

function Waves.parse_wave(text)
    local queue = {}
    for group in string.gmatch(text or "", "[^;]+") do
        local kind, count, gap, letter = string.match(group, "^%s*([%a_]+)%s+(%d+)%s+([%d%.]+)%s*(%a?)%s*$")
        if kind ~= nil and S.stats[kind] ~= nil and S.stats[kind].hp ~= nil then
            for i = 1, tonumber(count) do
                local road = letter == "b" and 2 or (letter == "a" and 1 or (#queue % 2) + 1)
                if S.routes[road] == nil then road = 1 end
                queue[#queue + 1] = { kind = kind, road = road,
                                      gap = (i == 1 and #queue > 0) and tonumber(gap) * 2 or tonumber(gap) }
            end
        else
            log.warn("Tower defence: wave group '" .. group .. "' is not 'kind count gap [road]'.")
        end
    end
    return queue
end

-- The first hour at or after the current one that reads `target` on the clock (the hours run on past midnight).
function Waves.next_hour(target)
    local h = math.floor(S.hours / 24.0) * 24.0 + target
    if h < S.hours then h = h + 24.0 end
    return h
end

-- Day or night for the next wave: the sky turns to dusk or dawn, the towers and the Horde take the effects of the
-- phase; a change between two waves is announced.
function Waves.set_phase(night, announce)
    if night then
        S.hours_goal = Waves.sky_is_night(S.hours) and S.hours + 0.3 or Waves.next_hour(night_hour)
    else
        S.hours_goal = Waves.sky_is_night(S.hours) and Waves.next_hour(morning_hour)
                or math.min(S.hours + 0.6, Waves.next_hour(17.5))
    end
    if night == Fx.night then return end
    Fx.night = night
    Fx.cache = {}
    for _, tower in ipairs(S.towers) do tower.stats = Towers.tower_stats(tower) end
    gamestate.set("tower_phase", night and "night" or "day")
    gamestate.set("tower_phase_changes", gamestate.get("tower_phase_changes", 0) + 1)
    Waves.mirror_effects()
    if announce then
        UI.play("sounds/game/horn.wav")
        UI.message(night and "Night falls\nthe next wave comes in the dark"
                or "Dawn breaks\nthe next wave comes by day", 2.8)
    end
end

function Waves.start_wave()
    S.wave = S.wave + 1
    S.spawning = Waves.parse_wave(waves[S.wave])
    S.spawn_clock = 0.0
    S.countdown = 0.0
    Waves.set_phase(Fx.nights[S.wave] == true, false)
    UI.play("sounds/game/horn.wav")
    UI.message("Wave " .. S.wave .. " / " .. S.wave_count .. (Fx.night and " - by night" or ""), 1.8)
end

function Waves.call_early()
    if S.ending ~= nil or S.spawning ~= nil or S.wave >= S.wave_count or S.route == nil then return end
    local bonus = math.floor(S.countdown * early_bonus)
    if bonus > 0 then
        S.gold = S.gold + bonus
        S.earned = S.earned + bonus
        S.bonus_total = S.bonus_total + bonus
        UI.play("sounds/game/coins.wav")
    end
    Waves.start_wave()
    if bonus > 0 then UI.message("Wave " .. S.wave .. " / " .. S.wave_count .. "\n+" .. bonus .. " gold", 1.8) end
end

function Waves.update_waves(dt)
    if S.route == nil then return end
    if S.spawning ~= nil then
        S.spawn_clock = S.spawn_clock - dt
        while S.spawning ~= nil and S.spawn_clock <= 0.0 do
            local next_one = table.remove(S.spawning, 1)
            if next_one == nil then
                S.spawning = nil
                if S.wave < S.wave_count then
                    S.countdown = wave_delay
                    Waves.set_phase(Fx.nights[S.wave + 1] == true, true)
                end
            else
                Foes.spawn_enemy(next_one.kind, next_one.road)
                S.spawn_clock = S.spawn_clock + (S.spawning[1] and S.spawning[1].gap or 0.0)
                if #S.spawning == 0 then
                    S.spawning = nil
                    if S.wave < S.wave_count then
                        S.countdown = wave_delay
                        Waves.set_phase(Fx.nights[S.wave + 1] == true, true)
                    end
                end
            end
        end
    elseif S.wave < S.wave_count then
        S.countdown = S.countdown - dt
        if S.countdown <= 0.0 then Waves.start_wave() end
    end
end

return Waves
