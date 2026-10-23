-- Victory screen of Tower defence, at the end of a map only: the stats of the map (`tower_result_*` in the game
-- state, written by defence.lua) counted up line by line with a tick each, the fireworks of parade.lua behind them.
-- Accept during the count shows every line at once; Play again replays the map, Title goes back.

properties = {
    { name = "line_time", type = "float", default = 0.7, min = 0.05 },
    { name = "wipe_color", type = "color", default = { 0.1, 0.14, 0.24, 1 } },
}

local lines = {}
local current, line_clock = 1, 0.0
local leaving = false

local function clock_text(seconds)
    return string.format("%d:%02d", seconds // 60, seconds % 60)
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("wipe_down", 0.5, wipe_color.r, wipe_color.g, wipe_color.b)
    end
    sound.play("sounds/ui/victory.wav")
    lines = {
        { label = "Enemies stopped ", value = gamestate.get("tower_result_kills", 0) },
        { label = "Lives kept      ", value = gamestate.get("tower_result_lives", 0),
          suffix = " / " .. gamestate.get("tower_result_max_lives", 20) },
        { label = "Gold earned     ", value = gamestate.get("tower_result_gold", 0) },
        { label = "Time            ", value = gamestate.get("tower_result_time", 0), time = true },
    }
    for index, line in ipairs(lines) do
        line.id = scene.find_entity("StatLine" .. index)
        if line.id ~= 0 then ui.set_text(line.id, "") end
    end
    gamestate.set("tower_tally_done", false)
end

local function text_of(line, value)
    local shown = line.time and clock_text(math.floor(value)) or tostring(math.floor(value))
    return line.label .. shown .. (line.suffix or "")
end

local function finish_count()
    for _, line in ipairs(lines) do
        if line.id ~= 0 then ui.set_text(line.id, text_of(line, line.value)) end
    end
    current = #lines + 1
    gamestate.set("tower_tally_done", true)
end

function on_update(dt)
    local real = time.real_delta()
    if current <= #lines then
        if input.is_action_just_pressed("ui_accept") then
            finish_count()
            return
        end
        line_clock = line_clock + real
        local line = lines[current]
        local t = math.min(1.0, line_clock / line_time)
        if line.id ~= 0 then ui.set_text(line.id, text_of(line, line.value * t)) end
        if t >= 1.0 then
            sound.play("sounds/ui/click.wav")
            current, line_clock = current + 1, 0.0
            if current > #lines then gamestate.set("tower_tally_done", true) end
        end
    end
end

local function leave(path)
    if leaving then return end
    leaving = true
    scene.transition_to(path, "wipe_down", 0.35, wipe_color.r, wipe_color.g, wipe_color.b)
end

function on_again_clicked()
    leave(gamestate.get("tower_scene", "scenes/map_01.owl"))
end

function on_title_clicked()
    leave("scenes/title.owl")
end
