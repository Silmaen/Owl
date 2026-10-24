-- The build wheel beside a pad: its badges, prices and info scroll.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local UI, Eco, Cam, Fx, Towers, Wheel = td.ui, td.economy, td.camera, td.effects, td.towers, td.wheel

function Wheel.close_ring()
    S.ring = nil
    S.hover = 0
    UI.show(S.ring_id, false)
    for _, id in ipairs(S.choice_ids) do UI.show(id, false) end
    for _, id in ipairs(S.wheel_ids.costs or {}) do UI.show(id, false) end
    local ids = S.wheel_ids
    for _, id in ipairs({ ids.ring, ids.plaque, ids.title, ids.text }) do UI.show(id, false) end
end

function Wheel.wrap(text)
    local lines, current = {}, ""
    for word in string.gmatch(text, "%S+") do
        if #current > 0 and #current + 1 + #word > C.WRAP then
            lines[#lines + 1] = current
            current = word
        else
            current = #current > 0 and (current .. " " .. word) or word
        end
    end
    if #current > 0 then lines[#lines + 1] = current end
    return table.concat(lines, "\n")
end

function Wheel.show_range(pad, range)
    if S.ring_id == 0 or range == nil then
        UI.show(S.ring_id, false)
        return
    end
    transform.set_position(S.ring_id, pad.x, pad.y + C.PAD_TOP + 0.03, pad.z)
    transform.set_scale(S.ring_id, range * 2.0, range * 2.0, 1.0)
    UI.show(S.ring_id, true)
end

-- The range a choice of the wheel would give.
function Wheel.range_of_choice(pad, choice)
    if choice == nil or choice.kind == "sell" then
        return pad.tower ~= nil and pad.tower.stats.range or nil
    end
    if pad.tower == nil then
        return Towers.tower_stats({ kind = choice.kind, level = 1, pad = pad }).range
    end
    local level = choice.kind == "upgrade" and pad.tower.level + 1 or pad.tower.level
    return Towers.tower_stats({ kind = pad.tower.kind, level = level, pad = pad }).range
end

function Wheel.choice_price(pad, choice)
    if choice.kind == "sell" then return 0 end
    if pad.tower == nil then return S.stats[choice.kind].cost end
    if choice.kind == "upgrade" then return Eco.step_cost(pad.tower.kind, pad.tower.level + 1) end
    return Eco.step_cost(pad.tower.kind, 4)
end

function Wheel.height_text(pad)
    if pad.level <= 0 then return "" end
    return string.format(" High ground: +%d %% range.", math.floor(height_bonus * pad.level * 100 + 0.5))
end

function Wheel.set_world_text(id, text)
    if id ~= 0 and S.wheel_shown[id] ~= text then
        ui.set_text(id, text)
        S.wheel_shown[id] = text
    end
end

-- The title and the text of the scroll for the choice under the cursor (or the pad itself).
function Wheel.info_of(pad, choice)
    local tower = pad.tower
    if choice == nil then
        if tower == nil then
            return "Build pad", "Choose a tower." .. Wheel.height_text(pad)
        end
        return C.NAMES[tower.kind] .. " - level " .. tower.level,
                (tower.power and (C.POWER_NAMES[tower.power] .. ". ") or "") .. Fx.time_text(tower.kind) .. "."
                .. Wheel.height_text(pad)
    end
    local kind = choice.kind
    local price = Wheel.choice_price(pad, choice)
    local short = S.gold < price and ("Not enough gold: " .. price - S.gold .. " more.") or nil
    if kind == "sell" then
        return "Sell for " .. Eco.refund_of(tower) .. " gold", "Half of what this tower cost."
    elseif kind == "upgrade" then
        local next_level = tower.level + 1
        return C.NAMES[tower.kind] .. " to level " .. next_level, short or C.LEVEL_TEXT[tower.kind][next_level]
    elseif C.POWER_NAMES[kind] ~= nil then
        if choice.locked then return C.POWER_NAMES[kind], "Unlocked by winning " .. C.UNLOCK_NAME .. "." end
        return C.POWER_NAMES[kind], short or C.POWER_TEXT[kind]
    end
    return C.NAMES[kind], (short or (C.BLURBS[kind] .. " " .. Fx.time_text(kind) .. ".")) .. Wheel.height_text(pad)
end

-- Place the wheel beside its pad on the screen, kept inside the view clear of the HUD, at a constant pixel size.
function Wheel.place_wheel()
    if S.ring == nil then return end
    local pad = S.ring.pad
    local lift = pad.tower ~= nil and C.SHOT_HEIGHT[pad.tower.level] * 0.6 or 0.6
    local ax, ay, az = pad.x, pad.y + lift, pad.z
    local px, py, unit, width, height = Cam.project(ax, ay, az)
    local half_w = math.max(C.WHEEL.ring, C.WHEEL.plaque_w) * 0.5
    local low, high = C.WHEEL.plaque_drop + C.WHEEL.plaque_h * 0.5, C.WHEEL.ring * 0.5
    local cx = math.max(C.WHEEL.margin + half_w, math.min(width - C.WHEEL.margin - half_w, px))
    local cy = math.max(height * 0.2 + high, math.min(height * 0.89 - low, py))
    local cwx, cwy, cwz = Cam.screen_offset(ax, ay, az, cx - px, py - cy, unit)
    S.ring.unit = unit
    transform.set_position(S.wheel_ids.ring, Cam.screen_offset(cwx, cwy, cwz, 0, 0, unit, 0.2))
    transform.set_scale(S.wheel_ids.ring, C.WHEEL.ring * unit, C.WHEEL.ring * unit, 1.0)
    local angles = C.WHEEL_ANGLES[#S.ring.choices] or C.WHEEL_ANGLES[4]
    for index, choice in ipairs(S.ring.choices) do
        local a = math.rad(angles[index])
        local bx, by = math.cos(a) * C.WHEEL.radius, math.sin(a) * C.WHEEL.radius
        transform.set_position(choice.id, Cam.screen_offset(cwx, cwy, cwz, bx, by, unit))
        local size = (index == S.hover and 1.18 or 1.0) * C.WHEEL.badge * unit
        transform.set_scale(choice.id, size, size, 1.0)
        local cost = S.wheel_ids.costs[index]
        if cost ~= nil then
            local price = Wheel.choice_price(pad, choice)
            local text = choice.kind == "sell" and ("+" .. Eco.refund_of(pad.tower))
                    or (choice.locked and "" or tostring(price))
            Wheel.set_world_text(cost, text)
            transform.set_position(cost, Cam.screen_offset(cwx, cwy, cwz, bx, by - C.WHEEL.cost_drop, unit, -0.1))
            transform.set_scale(cost, C.WHEEL.cost * unit, C.WHEEL.cost * unit, 1.0)
        end
    end
    local title, text = Wheel.info_of(pad, S.ring.choices[S.hover])
    Wheel.set_world_text(S.wheel_ids.title, title)
    Wheel.set_world_text(S.wheel_ids.text, Wheel.wrap(text))
    local top = -C.WHEEL.plaque_drop + C.WHEEL.plaque_h * 0.5
    transform.set_position(S.wheel_ids.plaque, Cam.screen_offset(cwx, cwy, cwz, 0, -C.WHEEL.plaque_drop, unit, 0.2))
    transform.set_scale(S.wheel_ids.plaque, C.WHEEL.plaque_w * unit, C.WHEEL.plaque_h * unit, 1.0)
    transform.set_position(S.wheel_ids.title, Cam.screen_offset(cwx, cwy, cwz, 0, top - 32, unit, -0.1))
    transform.set_scale(S.wheel_ids.title, C.WHEEL.title * unit, C.WHEEL.title * unit, 1.0)
    transform.set_position(S.wheel_ids.text, Cam.screen_offset(cwx, cwy, cwz, 0, top - 62, unit, -0.1))
    transform.set_scale(S.wheel_ids.text, C.WHEEL.text * unit, C.WHEEL.text * unit, 1.0)
    Wheel.show_range(pad, Wheel.range_of_choice(pad, S.ring.choices[S.hover]))
end

function Wheel.refresh_badges()
    if S.ring == nil then return end
    for _, choice in ipairs(S.ring.choices) do
        local afford = choice.kind == "sell" or (not choice.locked and S.gold >= Wheel.choice_price(S.ring.pad, choice))
        local base = afford and 1.0 or 0.5
        entity.set_tint(choice.id, base, base, base * (afford and 1.0 or 0.9), 1.0)
    end
end

function Wheel.open_ring(pad)
    Wheel.close_ring()
    S.ring = { pad = pad, choices = {} }
    local kinds = {}
    local tower = pad.tower
    if tower == nil then
        kinds = { { "archer" }, { "cannon" }, { "frost" } }
    elseif tower.level < 3 then
        kinds = { { "upgrade" }, { "sell" } }
    elseif tower.power == nil then
        local powers = C.POWERS[tower.kind]
        kinds = { { powers[1] }, { powers[2], not Eco.unlocked_second_power() }, { "sell" } }
    else
        kinds = { { "sell" } }
    end
    for index, entry in ipairs(kinds) do
        local id = S.choice_ids[index]
        if id ~= nil then
            local kind, locked = entry[1], entry[2] == true
            isometric.set_sprite_frame(id, locked and C.ICON.locked or C.ICON[kind])
            UI.show(id, true)
            UI.show(S.wheel_ids.costs[index], true)
            S.ring.choices[#S.ring.choices + 1] = { kind = kind, id = id, locked = locked }
        end
    end
    local ids = S.wheel_ids
    for _, id in ipairs({ ids.ring, ids.plaque, ids.title, ids.text }) do UI.show(id, true) end
    S.hover = S.pad_mode and 1 or 0
    UI.play("sounds/ui/click.wav")
    Wheel.refresh_badges()
    Wheel.place_wheel()
end

function Wheel.choose(index)
    if S.ring == nil then return end
    local choice = S.ring.choices[index]
    if choice == nil then return end
    if choice.kind == "sell" then
        Towers.sell(S.ring.pad)
    elseif S.ring.pad.tower == nil then
        Towers.build(S.ring.pad, choice.kind)
    else
        Towers.improve(S.ring.pad, choice)
    end
end

return Wheel
