-- Level select of Breakout, after the one of Sokoban: the five levels in a `UiList` with a cleared mark and the best
-- score made in each (the `breakout_done_N` and `breakout_best_N` settings written by game.lua). The rows are filled
-- when they come in view (`on_level_bind`); a click or up / down (D-pad) selects a level, Play, Enter or pad A on the
-- list starts a fresh run there (three lives, score 0) that goes on to the next levels; Back, Escape or pad B return
-- to the title.

-- Every level (a level names the next one in the `next_scene` property of its `Game` entity).
properties = {
    { name = "wipe_color", type = "color", default = { 0.45, 0.05, 0.55, 1 } },
}

local LEVEL_SCENES = {
    "scenes/level_1.owl",
    "scenes/level_2.owl",
    "scenes/level_3.owl",
    "scenes/level_4.owl",
    "scenes/level_5.owl",
}
local LEVEL_NAMES = { "First wall", "Pyramid", "Checkers", "Fortress", "Last stand" }

local list_id, info_id = 0, 0
local selected = 1
local leaving = false
local want_focus = true

local function done(i)
    return settings.get("breakout_done_" .. i, false)
end

local function best(i)
    return settings.get("breakout_best_" .. i, 0)
end

local function show_info()
    if info_id == 0 then return end
    local text = "LEVEL " .. selected .. "  " .. string.upper(LEVEL_NAMES[selected])
    if done(selected) then
        text = text .. "   BEST " .. best(selected)
    end
    ui.set_text(info_id, text)
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("wipe_up", 0.4, wipe_color.r, wipe_color.g, wipe_color.b)
    end
    list_id = scene.find_entity("LevelList")
    info_id = scene.find_entity("LevelInfo")
    -- Open on the first level not cleared yet.
    selected = 1
    for i = 1, #LEVEL_SCENES do
        if not done(i) then
            selected = i
            break
        end
    end
    ui.list_set_count(list_id, #LEVEL_SCENES)
    ui.list_set_selected(list_id, selected)
    gamestate.set("breakout_selected", selected)
    show_info()
end

-- Called for a row that comes in view (`_row_index`, 1-based).
function on_level_bind()
    local i = _row_index
    local mark = done(i) and "*  " or "    "
    local detail = done(i) and string.format("BEST %06d", best(i)) or "NOT CLEARED"
    ui.list_set_row(_list_entity, i, mark .. "LEVEL " .. i .. "   " .. string.upper(LEVEL_NAMES[i] or ""), detail)
end

-- Called when the selection changes (`_selected_index`).
function on_level_selected()
    if _selected_index >= 1 and _selected_index <= #LEVEL_SCENES then
        selected = _selected_index
        gamestate.set("breakout_selected", selected)
        show_info()
    end
end

function on_play_clicked()
    if leaving then return end
    leaving = true
    sound.play("sounds/ui/start.wav")
    gamestate.clear()
    gamestate.set("breakout_score", 0)
    gamestate.set("breakout_lives", 3)
    gamestate.set("breakout_level", selected)
    gamestate.set("breakout_scene", LEVEL_SCENES[selected])
    scene.transition_to(LEVEL_SCENES[selected], "wipe_up", 0.3, wipe_color.r, wipe_color.g, wipe_color.b)
end

function go_back()
    if leaving then return end
    leaving = true
    scene.transition_to("scenes/title.owl", "wipe_up", 0.3, wipe_color.r, wipe_color.g, wipe_color.b)
end

function on_update(dt)
    if leaving then return end
    -- The list is laid out on the first frame: take the focus once it can hold it.
    if want_focus and ui.set_focus(list_id) then
        want_focus = false
    end
    if input.is_action_just_pressed("ui_cancel") then
        go_back()
    elseif input.is_action_just_pressed("ui_accept") and ui.get_focus() == list_id then
        on_play_clicked()
    end
end
