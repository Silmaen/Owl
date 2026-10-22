-- Inventory window of the raycast level (the window of huds/raycast.owlhud).
-- Demonstrates: an `.owlhud` of its own driven from Lua (slots, icons, counts, tooltips), a window opened by an input
--               action, keyboard and gamepad focus (ui.set_focus, onFocusCallback, ui_* actions), onCloseCallback,
--               gamestate as the inventory (kept in saves), save.save_game / save.load_game, scene.transition_to,
--               a scrolling journal (UiList: ui.list_set_count, onBindCallback + ui.list_set_row, onSelectCallback).
--
-- The inventory is the gamestate keys `inv_<item>` (counts), written by pickup.lua. One slot per item:
-- its icon and count show once the count is above zero. `inv_window_open` and `inv_focus` (slot index) mirror the UI
-- for the other scripts and the scenario test; `inv_closed_frame` is the `inv_frame` at which ui_cancel closed it, so the
-- player script does not take that Escape for a pause.

properties = {
    { name = "toast_seconds", type = "float", default = 2.5, min = 0 },
}

local ITEMS = {
    { id = "cross", name = "Golden cross", info = "Treasure: a holy relic." },
    { id = "chalice", name = "Chalice", info = "Treasure: solid gold." },
    { id = "chest", name = "Treasure chest", info = "Treasure: shiny, and worth a lot." },
    { id = "crown", name = "Crown", info = "Treasure: the prize of a secret room." },
    { id = "food", name = "Roast dinner", info = "A warm meal." },
    { id = "dogfood", name = "Dog food", info = "Not for you, but it fills a belly." },
    { id = "health", name = "First-aid kit", info = "Patches you up." },
    { id = "ammo", name = "Ammo clip", info = "Eight rounds." },
    { id = "gun", name = "Machine gun", info = "Rattles the walls." },
}
local SAVE_SLOT = 2
local window_id, info_id, toast_id, log_id = 0, 0, 0, 0
local slots, icons, counts = {}, {}, {}
local slot_index = {}  -- slot entity id -> index in ITEMS
local open = false
local want_focus = false
local toast_left = 0
local frame = 0

local function count_of(i)
    return gamestate.get("inv_" .. ITEMS[i].id, 0)
end

local function refresh()
    for i = 1, #ITEMS do
        local n = count_of(i)
        ui.set_visible(icons[i], n > 0)
        ui.set_visible(counts[i], n > 0)
        ui.set_text(counts[i], tostring(n))
    end
end

local function show_info(i)
    local item = ITEMS[i]
    local n = count_of(i)
    local status = n > 0 and ("x" .. n) or "none"
    ui.set_text(info_id, item.name .. " (" .. status .. ")\n" .. item.info)
end

local function first_filled_slot()
    for i = 1, #ITEMS do
        if count_of(i) > 0 then
            return i
        end
    end
    return 1
end

local function set_open(value)
    open = value
    ui.set_visible(window_id, value)
    gamestate.set("inv_window_open", value and 1 or 0)
    if value then
        refresh()
        want_focus = true
    end
end

local log_lines = {}

-- The journal keeps every message; the list only asks for the rows in view (`on_log_bind`).
local function log_add(text)
    log_lines[#log_lines + 1] = text
    ui.list_set_count(log_id, #log_lines)
    ui.scroll_set(log_id, 0, #log_lines * 24)
end

local function toast(text)
    log_add(text)
    ui.set_text(toast_id, text)
    ui.set_visible(toast_id, true)
    toast_left = toast_seconds
end

function on_create()
    window_id = ui.find("InventoryWindow")
    info_id = ui.find("InventoryInfo")
    toast_id = ui.find("InventoryToast")
    log_id = ui.find("InventoryLog")
    for i, item in ipairs(ITEMS) do
        slots[i] = ui.find("InvSlot" .. i)
        icons[i] = ui.find("InvIcon" .. i)
        counts[i] = ui.find("InvCount" .. i)
        slot_index[slots[i]] = i
    end
    ui.set_visible(toast_id, false)
    set_open(false)
    refresh()
    ui.transition_fade_in(0.4)
end

-- Called when the close button or ui_cancel (Escape, gamepad B) closes the window.
function on_inventory_closed()
    open = false
    gamestate.set("inv_window_open", 0)
    gamestate.set("inv_closed_frame", gamestate.get("inv_frame", 0))
end

-- Called when a slot takes the focus (`_focused_entity`): the info text follows it.
function on_slot_focused()
    local i = slot_index[_focused_entity]
    if i then
        gamestate.set("inv_focus", i)
        show_info(i)
    end
end

-- Called when a slot is clicked or accepted (`_clicked_entity`).
function on_slot_clicked()
    local i = slot_index[_clicked_entity]
    if i then
        gamestate.set("inv_selected", i)
        show_info(i)
    end
end

-- Called for a journal row that comes in view (`_list_entity`, `_row_index`, 1-based).
function on_log_bind()
    ui.list_set_row(_list_entity, _row_index, log_lines[_row_index] or "", tostring(_row_index))
end

-- Called when a journal row is selected (`_selected_index`): the info text shows it in full.
function on_log_selected()
    local line = log_lines[_selected_index]
    if line then
        ui.set_text(info_id, line)
    end
end

function on_update(dt)
    if ui.is_transition_active() then
        return
    end
    frame = frame + 1
    gamestate.set("inv_frame", frame)

    -- A save restores the list size but not the journal lines of this script: keep the two in step.
    if ui.list_get_count(log_id) ~= #log_lines then
        ui.list_set_count(log_id, #log_lines)
        ui.list_invalidate(log_id)
    end

    -- Messages of the pickups and the locks.
    local message = gamestate.get("inv_toast", "")
    if message ~= "" then
        gamestate.set("inv_toast", "")
        toast(message)
    end
    if toast_left > 0 then
        toast_left = toast_left - dt
        if toast_left <= 0 then
            ui.set_visible(toast_id, false)
        end
    end

    if input.is_action_just_pressed("inventory") then
        set_open(not open)
    end
    if open then
        refresh()
        -- The window is laid out on the frame it appears: take the focus once the slot can hold it.
        if want_focus and ui.set_focus(slots[first_filled_slot()]) then
            want_focus = false
        end
    end

    if input.is_action_just_pressed("quick_save") then
        save.save_game(SAVE_SLOT)
        toast("Game saved")
    elseif input.is_action_just_pressed("quick_load") and save.has_save(SAVE_SLOT) then
        save.load_game(SAVE_SLOT)
    end
end
