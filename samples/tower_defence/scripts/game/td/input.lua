-- Mouse, keyboard and gamepad: the pad cursor, clicks and the actions of a level.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local UI, Waves, Towers, Spells, Wheel, Input = td.ui, td.waves, td.towers, td.spells, td.wheel, td.input

function Input.pad_of_tower(id)
    for _, tower in ipairs(S.towers) do
        if tower.id == id then return tower.pad end
    end
    return nil
end

function Input.pad_under_mouse()
    local pad = Input.pad_of_tower(scene.pick_entity(nil, nil, "Tower"))
    if pad == nil then pad = S.pad_by_id[scene.pick_entity(nil, nil, "Pad")] end
    return pad
end

function Input.click()
    if S.ring ~= nil then
        local badge = scene.pick_entity(nil, nil, "BuildChoice")
        for index, choice in ipairs(S.ring.choices) do
            if choice.id == badge then
                Wheel.choose(index)
                return
            end
        end
    end
    local pad = Input.pad_under_mouse()
    if pad ~= nil and (S.ring == nil or S.ring.pad ~= pad) then
        S.cursor_pad = pad
        Wheel.open_ring(pad)
    else
        Wheel.close_ring()
    end
end

function Input.hover_badge()
    if S.ring == nil or S.pad_mode then return end
    local badge = scene.pick_entity(nil, nil, "BuildChoice")
    local found = 0
    for index, choice in ipairs(S.ring.choices) do
        if choice.id == badge then found = index end
    end
    if found ~= S.hover then
        S.hover = found
        Wheel.place_wheel()
    end
end

-- The pads from the left of the screen to the right (then from the back to the front).
function Input.pads_on_screen()
    local order = {}
    for _, pad in ipairs(S.pads) do order[#order + 1] = pad end
    local r, t = S.basis.right, S.basis.toward
    table.sort(order, function(a, b)
        local sa, sb = a.x * r[1] + a.z * r[3], b.x * r[1] + b.z * r[3]
        if math.abs(sa - sb) > 0.01 then return sa < sb end
        return a.x * t[1] + a.z * t[2] < b.x * t[1] + b.z * t[2]
    end)
    return order
end

function Input.step_cursor(direction)
    local order = Input.pads_on_screen()
    if #order == 0 then return end
    local index = 0
    for i, pad in ipairs(order) do
        if pad == S.cursor_pad then index = i end
    end
    if index == 0 then
        index = direction > 0 and 1 or #order
    else
        index = (index - 1 + direction) % #order + 1
    end
    S.cursor_pad = order[index]
end

function Input.update_cursor()
    local mx, my = scene.get_viewport_mouse()
    if S.last_mouse_x ~= nil and (mx ~= S.last_mouse_x or my ~= S.last_mouse_y) then
        S.mouse_moved = true
        S.pad_mode = false
    end
    S.last_mouse_x, S.last_mouse_y = mx, my
    local shown_pad = nil
    if S.pad_mode then
        shown_pad = S.cursor_pad
    elseif S.mouse_moved and not ui.is_mouse_over_ui() then
        shown_pad = Input.pad_under_mouse()
    end
    if shown_pad ~= nil and S.cursor_id ~= 0 then
        transform.set_position(S.cursor_id, shown_pad.x, shown_pad.y + C.PAD_TOP + 0.02, shown_pad.z)
        transform.set_scale(S.cursor_id, 1.05, 1.05, 1.0)
    end
    UI.show(S.cursor_id, shown_pad ~= nil)
    gamestate.set("tower_hover_pad", shown_pad and shown_pad.index or 0)
end

function Input.handle_input()
    if input.is_action_just_pressed("fast_forward") then on_speed_clicked() end
    if input.is_action_just_pressed("call_wave") then Waves.call_early() end
    if input.is_action_just_pressed("rain") then Spells.cast("rain") end
    if input.is_action_just_pressed("shield") then Spells.cast("shield") end
    local world_free = not ui.is_world_blocked()
    if world_free and input.is_action_just_pressed("primary") and not ui.is_mouse_over_ui() then
        S.pad_mode = false
        Input.click()
    end
    if input.is_action_just_pressed("secondary") or input.is_action_just_pressed("cancel") then Wheel.close_ring() end
    local step = 0
    if input.is_action_just_pressed("next_pad") then step = 1 end
    if input.is_action_just_pressed("prev_pad") then step = -1 end
    if step ~= 0 then
        S.pad_mode = true
        if S.ring ~= nil and #S.ring.choices > 1 then
            S.hover = (math.max(S.hover, 1) - 1 + step) % #S.ring.choices + 1
            Wheel.place_wheel()
        else
            Wheel.close_ring()
            Input.step_cursor(step)
        end
    end
    if input.is_action_just_pressed("confirm") and S.cursor_pad ~= nil then
        S.pad_mode = true
        if S.ring ~= nil and S.hover > 0 then
            Wheel.choose(S.hover)
        else
            Wheel.open_ring(S.cursor_pad)
        end
    end
    for index = 1, 3 do
        if input.is_action_just_pressed("build_" .. index) and S.ring ~= nil and S.ring.choices[index] ~= nil
                and S.ring.choices[index].kind ~= "sell" then
            Wheel.choose(index)
        end
    end
    if input.is_action_just_pressed("sell") and S.ring ~= nil and S.ring.pad.tower ~= nil then
        Towers.sell(S.ring.pad)
    end
    if input.is_action_just_pressed("turn_left") then S.view.yaw_goal = S.view.yaw_goal + math.pi / 4 end
    if input.is_action_just_pressed("turn_right") then S.view.yaw_goal = S.view.yaw_goal - math.pi / 4 end
end

return Input
