-- Victory screen of Ring Runner: the stats of the flight (`rail_result_*` game-state keys) come one line after the
-- other, each number counted up, then the medal grows into place; the best score and its medal are kept in the
-- settings (`rail_best`, `rail_best_medal`). Fly again restarts the level, Title goes back to the title.

properties = {
    { name = "line_time", type = "float", default = 0.55, min = 0.05 },
    { name = "sweep_color", type = "color", default = { 0.0, 0.05, 0.14, 1 } },
}

local MEDALS = { "BRONZE", "SILVER", "GOLD" }
local lines = {}
local line_ids = {}
local medal_id, best_id = 0, 0
local best_label = ""
local medal, clock, shown_line = 1, 0.0, 0
local leaving = false

local function line(label, value, format, suffix)
    lines[#lines + 1] = { label = label, value = value, format = format or "%d", suffix = suffix or "" }
end

local function write(index, fraction)
    local entry = lines[index]
    local value = math.floor(entry.value * fraction + 0.5)
    local number = string.format(entry.format, value) .. entry.suffix
    ui.set_text(line_ids[index], string.format("%-16s %10s", entry.label, number))
end

function on_create()
    if not ui.is_transition_active() then ui.transition_play("fade", 0.8, 0.9, 0.95, 1.0) end
    sound.play("sounds/ui/victory.wav")
    local score = gamestate.get("rail_result_score", 0)
    medal = gamestate.get("rail_result_medal", 1)
    line("SCORE", score, "%06d")
    line("HULL BONUS", gamestate.get("rail_result_bonus", 0))
    line("ENEMIES DOWN", gamestate.get("rail_result_kills", 0), "%d", " / " .. gamestate.get("rail_result_foes", 0))
    line("ROCKS BROKEN", gamestate.get("rail_result_rocks", 0))
    line("HIT RATIO", gamestate.get("rail_result_hits", 0), "%d", "%")
    line("HULL LEFT", gamestate.get("rail_result_hull", 0), "%d", "%")
    for index = 1, #lines do line_ids[index] = scene.find_entity("StatLine" .. index) end
    medal_id = scene.find_entity("Medal")
    best_id = scene.find_entity("BestLabel")
    if medal_id ~= 0 then
        ui.set_image_frame(medal_id, medal)
        ui.set_visible(medal_id, false)
    end
    local best = settings.get("rail_best", 0)
    local best_text = string.format("BEST %06d", best)
    if score > best then
        settings.set("rail_best", score)
        best_text = "NEW BEST SCORE!"
    end
    if medal > settings.get("rail_best_medal", 0) then settings.set("rail_best_medal", medal) end
    settings.save()
    if best_id ~= 0 then ui.set_text(best_id, "") end
    best_label = best_text
    gamestate.set("rail_tally_done", false)
end

function on_update(dt)
    clock = clock + dt
    -- Each line shows up after the previous one and counts its number up for most of its time.
    local current = math.min(#lines, math.floor(clock / line_time) + 1)
    for index = shown_line + 1, current do
        if line_ids[index] ~= 0 then ui.set_visible(line_ids[index], true) end
        sound.play("sounds/ui/select.wav")
    end
    shown_line = math.max(shown_line, current)
    for index = 1, shown_line do
        if line_ids[index] ~= 0 then
            write(index, math.min(1, (clock - (index - 1) * line_time) / (line_time * 0.8)))
        end
    end
    -- Then the medal grows from nothing to its size, a little past it and back.
    local medal_clock = clock - #lines * line_time
    if medal_clock > 0 and medal_id ~= 0 then
        ui.set_visible(medal_id, true)
        local t = math.min(1, medal_clock / 0.5)
        local size = 0.16 * (t < 0.8 and t / 0.8 * 1.15 or 1.15 - (t - 0.8) / 0.2 * 0.15)
        ui.set_size(medal_id, size, size / 0.16)
        if t >= 1 and not gamestate.get("rail_tally_done", false) then
            gamestate.set("rail_tally_done", true)
            gamestate.set("rail_medal_shown", MEDALS[medal] or "")
            if best_id ~= 0 then ui.set_text(best_id, MEDALS[medal] .. " MEDAL - " .. best_label) end
        end
    end
end

local function leave(path)
    if leaving then return end
    leaving = true
    gamestate.clear()
    scene.transition_to(path, "wipe_left", 0.45, sweep_color.r, sweep_color.g, sweep_color.b)
end

function on_retry_clicked()
    leave("scenes/level.owl")
end

function on_menu_clicked()
    leave("scenes/title.owl")
end
