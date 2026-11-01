-- Level select of Sokoban: the ten levels in a `UiList` (a done mark and the best moves from the settings), after the
-- journal of the Engine Demo inventory (scripts/raycast/inventory.lua): the rows are filled when they come in view
-- (`on_level_bind`). A click or the up / down keys (D-pad) select a level; Play, Enter or pad A on the list start it;
-- Back, Escape or pad B return to the title. The levels are the rows of `tables/levels.owltable` (scripts/game/levels.lua).

local levels = nil              -- scripts/game/levels.lua, required in on_create
local list_id, info_id = 0, 0
local selected = 1
local leaving = false
local want_focus = true

local function done(i)
    return levels.done(i)
end

local function best(i)
    return levels.best(i)
end

local function show_info()
    if info_id == 0 then return end
    local text = "Level " .. selected .. ": " .. levels.name(selected)
    if done(selected) then
        text = text .. "   done in " .. best(selected) .. " moves"
    end
    ui.set_text(info_id, text)
end

function on_create()
    levels = require("scripts.game.levels")
    if not ui.is_transition_active() then ui.transition_play("wipe_left", 0.4, 0.2, 0.12, 0.05) end
    list_id = scene.find_entity("LevelList")
    info_id = scene.find_entity("LevelInfo")
    -- Open on the first level not done yet.
    selected = 1
    for i = 1, levels.count() do
        if not done(i) then
            selected = i
            break
        end
    end
    ui.list_set_count(list_id, levels.count())
    ui.list_set_selected(list_id, selected)
    gamestate.set("sokoban_selected", selected)
    show_info()
end

-- Called for a row that comes in view (`_row_index`, 1-based).
function on_level_bind()
    local i = _row_index
    local detail = done(i) and ("done, best " .. best(i)) or ""
    local mark = done(i) and "[x] " or "[  ] "
    ui.list_set_row(_list_entity, i, mark .. i .. ". " .. levels.name(i), detail)
end

-- Called when the selection changes (`_selected_index`).
function on_level_selected()
    if _selected_index >= 1 and _selected_index <= levels.count() then
        selected = _selected_index
        gamestate.set("sokoban_selected", selected)
        show_info()
    end
end

function on_play_clicked()
    if leaving then return end
    leaving = true
    sound.play("sounds/ui/start.wav")
    gamestate.set("sokoban_level", selected)
    scene.transition_to(levels.scene(selected), "wipe_left", 0.3, 0.2, 0.12, 0.05)
end

function go_back()
    if leaving then return end
    leaving = true
    scene.transition_to("scenes/title.owl", "wipe_left", 0.3, 0.2, 0.12, 0.05)
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
