-- Tally screen of Wolfenstein mini, between two floors: what exit.lua wrote into `wolf_tally_*`, counted up one line
-- after the other (items, secrets, map explored, then the bonus), a tick while a line counts and a thump when it is
-- done, then the scores and the best ratios of the floor (settings `wolf_best_<floor>_*`). Enter, Space, a click or
-- pad A during the count shows everything at once; the Next button (the next floor in `wolf_next_scene`, the victory
-- after the last one) is enabled once the count is over. `wolf_tally_step` counts the lines done (4 at the end).

local LINES = {
    { id = "ItemsLabel", name = "ITEMS", key = "wolf_tally_items", unit = "%" },
    { id = "SecretsLabel", name = "SECRETS", key = "wolf_tally_secrets", unit = "%" },
    { id = "MapLabel", name = "MAP EXPLORED", key = "wolf_tally_explored", unit = "%" },
    { id = "BonusLabel", name = "BONUS", key = "wolf_tally_bonus", unit = "" },
}
local COUNT_RATE = { ["%"] = 80, [""] = 2400 } -- units per second while a line counts
local PAUSE = 0.35                            -- seconds between two lines
local TICK_EVERY = { ["%"] = 5, [""] = 150 }
local FADE = { 0.22, 0.02, 0.02 }

local ids = {}
local line, shown, wait = 1, 0, 0.6
local last_tick = 0
local done = false
local enable_next = false
local leaving = false

local function format(entry, value)
    return string.format("%-14s %5d%s", entry.name, value, entry.unit)
end

local function set(name, text)
    if ids[name] ~= nil and ids[name] ~= 0 then
        ui.set_text(ids[name], text)
    end
end

local function finish_line(entry)
    set(entry.id, format(entry, gamestate.get(entry.key, 0)))
    gamestate.set("wolf_tally_step", line)
end

local function show_scores()
    local level = gamestate.get("wolf_tally_level", 1)
    local best = "wolf_best_" .. level .. "_"
    set("ScoreLabel", string.format("FLOOR SCORE %d     TOTAL %d", gamestate.get("wolf_tally_floor_score", 0),
            gamestate.get("wolf_score", 0)))
    set("BestLabel", string.format("Best on this floor: items %d%%, secrets %d%%, map %d%%",
            settings.get(best .. "items", 0), settings.get(best .. "secrets", 0), settings.get(best .. "explored", 0)))
    set("SkipHint", "")
    done = true
    enable_next = true
    gamestate.set("wolf_tally_done", true)
    sound.play("sounds/ui/tally_total.wav")
end

-- Everything at once (a key pressed during the count).
local function skip()
    for index = line, #LINES do
        line = index
        finish_line(LINES[index])
    end
    line = #LINES + 1
    show_scores()
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("fade_in", 0.5, FADE[1], FADE[2], FADE[3])
    end
    for _, name in ipairs({ "TallyTitle", "ItemsLabel", "SecretsLabel", "MapLabel", "BonusLabel", "ScoreLabel",
                            "BestLabel", "SkipHint", "NextButton" }) do
        ids[name] = scene.find_entity(name)
    end
    set("TallyTitle", "FLOOR " .. gamestate.get("wolf_tally_level", 1) .. " COMPLETED")
    for _, entry in ipairs(LINES) do
        set(entry.id, "")
    end
    set("ScoreLabel", "")
    set("BestLabel", "")
    gamestate.set("wolf_tally_step", 0)
    gamestate.set("wolf_tally_done", false)
    if gamestate.get("wolf_next_scene", "") == "scenes/victory.owl" then
        set("NextButton", "FINISH")
    end
    if ids.NextButton ~= 0 then
        ui.set_button_enabled(ids.NextButton, false)
    end
end

function on_update(dt)
    if done then
        -- Enabled one frame after the end, so that the key that skipped the count does not press Next as well.
        if enable_next then
            enable_next = false
        elseif ids.NextButton ~= 0 and not leaving and ui.get_focus() ~= ids.NextButton then
            ui.set_button_enabled(ids.NextButton, true)
            ui.set_focus(ids.NextButton)
        end
        return
    end
    if ui.is_transition_active() then
        return
    end
    if input.is_action_just_pressed("ui_accept") or input.is_action_just_pressed("interact") then
        sound.play("sounds/ui/tally_line.wav")
        skip()
        return
    end
    if wait > 0 then
        wait = wait - dt
        return
    end
    local entry = LINES[line]
    local target = gamestate.get(entry.key, 0)
    shown = math.min(target, shown + COUNT_RATE[entry.unit] * dt)
    local value = math.floor(shown)
    set(entry.id, format(entry, value))
    if value - last_tick >= TICK_EVERY[entry.unit] then
        last_tick = value
        sound.play("sounds/ui/tally_tick.wav")
    end
    if shown >= target then
        finish_line(entry)
        sound.play("sounds/ui/tally_line.wav")
        line, shown, last_tick, wait = line + 1, 0, 0, PAUSE
        if line > #LINES then
            show_scores()
        end
    end
end

function on_next_clicked()
    if leaving or not done then
        return
    end
    leaving = true
    sound.play("sounds/ui/start.wav")
    scene.transition_to(gamestate.get("wolf_next_scene", "scenes/title.owl"), "fade_out", 0.5, FADE[1], FADE[2],
            FADE[3])
end
