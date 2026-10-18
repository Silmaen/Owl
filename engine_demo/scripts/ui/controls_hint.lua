-- Short-lived controls hint of a level: the HUD text shows the keys when the level starts, then hides after
-- `duration` seconds. The rule of the demo: controls are never shown for good in game, only in the main menu (the
-- pause: Escape leads there and Continue comes back) and in this hint.
-- Demonstrates: ui.find, ui.set_visible, a timer kept in Lua, gamestate as a probe for the scenarios.

properties = {
    { name = "hint", type = "string", default = "HudHint" },
    { name = "duration", type = "float", default = 4.5 },
}

local hint_id = 0
local clock = 0
local shown = false

function on_create()
    hint_id = ui.find(hint)
    shown = hint_id ~= 0
    if shown then
        ui.set_visible(hint_id, true)
    end
    gamestate.set("controls_hint_shown", shown)
end

function on_update(dt)
    if not shown then
        return
    end
    clock = clock + dt
    if clock >= duration then
        shown = false
        ui.set_visible(hint_id, false)
        gamestate.set("controls_hint_shown", false)
    end
end
