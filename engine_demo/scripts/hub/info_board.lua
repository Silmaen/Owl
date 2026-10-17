-- Information board of the hub (the sign near the start): E opens the window of huds/world_map.owlhud.
-- Demonstrates: a window shown and hidden from Lua (`ui.set_visible`, `ui.raise_window`), its `onCloseCallback`, a
--               `UiScrollView` of help text scrolled by the wheel, the bar or the `move` action (`ui.scroll_get` /
--               `ui.scroll_set`), a `Trigger.Type=Interaction` callback.
--
-- `board_open` mirrors the window for the other scripts and the scenario test, `board_scroll` is the scroll offset, and
-- `board_closed_frame` is the `board_frame` at which the window was closed, so the player script does not take the
-- Escape that closed the window for a pause.

local SCROLL_SPEED = 420

local window_id, scroll_id = 0, 0
local open = false
local frame = 0

local function set_open(value)
    open = value
    ui.set_visible(window_id, value)
    if value then
        ui.raise_window(window_id)
    end
    sound.play("sounds/hub/board.wav")
    gamestate.set("board_open", value and 1 or 0)
    if not value then
        gamestate.set("board_closed_frame", frame)
    end
end

function on_create()
    window_id = ui.find("BoardWindow")
    scroll_id = ui.find("BoardScroll")
    gamestate.set("board_open", 0)
    gamestate.set("board_scroll", 0)
    gamestate.set("board_frame", 0)
    gamestate.set("board_closed_frame", -100)
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_board_interact: E toggles the window.
function on_board_interact(player_id)
    set_open(not open)
end

-- Called when the close button or ui_cancel (Escape) closes the window.
function on_board_closed()
    open = false
    sound.play("sounds/hub/board.wav")
    gamestate.set("board_open", 0)
    gamestate.set("board_closed_frame", frame)
end

function on_update(dt)
    frame = frame + 1
    gamestate.set("board_frame", frame)
    if not open then
        return
    end
    if input.is_action_just_pressed("ui_cancel") then
        set_open(false)
        return
    end
    local _, dy = input.get_action_vector("move")
    local _, y = ui.scroll_get(scroll_id)
    if dy ~= 0 then
        y = y + dy * SCROLL_SPEED * dt
        ui.scroll_set(scroll_id, 0, y)
        _, y = ui.scroll_get(scroll_id)
    end
    gamestate.set("board_scroll", y)
end
