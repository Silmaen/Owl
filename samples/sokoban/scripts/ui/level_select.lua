-- Level select of Sokoban: the ten levels in a `UiList` (a done mark and the best moves from the settings), after the
-- journal of the Engine Demo inventory (scripts/raycast/inventory.lua): the rows are filled when they come in view
-- (`on_level_bind`). A click or the up / down keys (D-pad) select a level; Play, Enter or pad A on the list start it;
-- Back, Escape or pad B return to the title.

local LEVEL_SCENES = {
    "scenes/level_01.owl",
    "scenes/level_02.owl",
    "scenes/level_03.owl",
    "scenes/level_04.owl",
    "scenes/level_05.owl",
    "scenes/level_06.owl",
    "scenes/level_07.owl",
    "scenes/level_08.owl",
    "scenes/level_09.owl",
    "scenes/level_10.owl",
}
local LEVEL_NAMES = { "First push", "Two by two", "Side door", "Back room", "Cellar", "Four corners", "Pantry",
                      "Columns", "Loading bay", "Warehouse" }

local list_id, info_id = 0, 0
local selected = 1
local leaving = false
local want_focus = true

local function done(i)
    return settings.get(string.format("sokoban_done_%02d", i), false)
end

local function best(i)
    return settings.get(string.format("sokoban_best_%02d", i), 0)
end

local function show_info()
    if info_id == 0 then return end
    local text = "Level " .. selected .. ": " .. LEVEL_NAMES[selected]
    if done(selected) then
        text = text .. "   done in " .. best(selected) .. " moves"
    end
    ui.set_text(info_id, text)
end

function on_create()
    ui.transition_fade_in(0.4)
    list_id = scene.find_entity("LevelList")
    info_id = scene.find_entity("LevelInfo")
    -- Open on the first level not done yet.
    selected = 1
    for i = 1, #LEVEL_SCENES do
        if not done(i) then
            selected = i
            break
        end
    end
    ui.list_set_count(list_id, #LEVEL_SCENES)
    ui.list_set_selected(list_id, selected)
    gamestate.set("sokoban_selected", selected)
    show_info()
end

-- Called for a row that comes in view (`_row_index`, 1-based).
function on_level_bind()
    local i = _row_index
    local detail = done(i) and ("done, best " .. best(i)) or ""
    local mark = done(i) and "[x] " or "[  ] "
    ui.list_set_row(_list_entity, i, mark .. i .. ". " .. (LEVEL_NAMES[i] or ""), detail)
end

-- Called when the selection changes (`_selected_index`).
function on_level_selected()
    if _selected_index >= 1 and _selected_index <= #LEVEL_SCENES then
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
    scene.transition_to(LEVEL_SCENES[selected], "fade_out", 0.3)
end

function go_back()
    if leaving then return end
    leaving = true
    scene.transition_to("scenes/title.owl", "fade_out", 0.3)
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
