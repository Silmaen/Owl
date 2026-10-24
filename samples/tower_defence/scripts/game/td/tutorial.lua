-- The tutorial of the first level: short guided steps, each a parchment hint beside its target with an arrow, waiting
-- for the action while the time runs slowly: build a tower on the glowing pad, call a wave early, raise the tower,
-- sell it, then read the day and night of the HUD. Shown while the `tower_tutorial` setting is empty; finishing it
-- sets `done`, the Skip tutorial buttons of the briefing and of the pause window (or pad B / Backspace on the
-- briefing) set `skipped`; Reset tutorial in the settings empties it again.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local UI, Cam, Eco, Tut = td.ui, td.camera, td.economy, td.tutorial

Tut.step = "off"
Tut.ids = {}
Tut.clock = 0.0
Tut.pad = nil
Tut.baseline = {}

local SLOW = 0.2                     -- the time scale while a step waits
local PLAQUE_W, PLAQUE_H = 380, 136  -- pixels of the hint scroll
local ARROW = 84                     -- pixels of the arrow
local STEPS = { "build", "call", "upgrade", "sell", "night" }
local TEXTS = {
    build = { "Build a tower", "Click the glowing pad (or E, then Enter) and pick the archer badge." },
    call = { "Call the first wave", "Press Call wave (Space): an early call pays gold for every second left." },
    upgrade = { "Raise your tower", "Click your tower again and pick the up arrow: level 2 shoots harder and further." },
    sell = { "Sell a tower", "Click a tower and pick the coin badge: it pays back half of what it cost." },
    night = { "Day and night", "The sun or the moon here: by night archers see less far, frost bites harder and the "
            .. "Horde runs faster. The pause window lists every effect. Click or press Enter." },
}

local function setting()
    return settings.get("tower_tutorial", "")
end

-- Whether the tutorial is offered on this level: the first one, while it was neither finished nor skipped.
function Tut.offered()
    return tutorial and setting() == ""
end

local function show_hint(visible)
    for _, key in ipairs({ "plaque", "title", "text", "arrow", "glow" }) do UI.show(Tut.ids[key], visible) end
end

local function mirror()
    gamestate.set("tower_tutorial_step", Tut.step)
    gamestate.set("tower_tutorial_state", setting())
end

local function finish(state)
    settings.set("tower_tutorial", state)
    settings.save()
    Tut.step = "off"
    show_hint(false)
    time.set_scale(S.fast and 2.0 or 1.0)
    UI.show(S.hud.skip_tutorial, false)
    mirror()
end

local function enter(step)
    Tut.step = step
    Tut.clock = 0.0
    Tut.baseline = { built = S.built, upgrades = S.upgrades, wave = S.wave, towers = #S.towers }
    if step == "done" then
        finish("done")
        UI.message("Tutorial done\nthe keep is yours to defend", 2.5)
        return
    end
    sound.play("sounds/ui/click.wav")
    time.set_scale(step == "call" and 1.0 or SLOW)
    local title, text = TEXTS[step][1], TEXTS[step][2]
    td.wheel.set_world_text(Tut.ids.title, title)
    td.wheel.set_world_text(Tut.ids.text, td.wheel.wrap(text))
    show_hint(true)
    mirror()
end

local function next_step()
    for index, step in ipairs(STEPS) do
        if step == Tut.step then
            enter(STEPS[index + 1] or "done")
            return
        end
    end
end

function Tut.start()
    Tut.ids = { plaque = scene.find_entity("TutorialPlaque"), title = scene.find_entity("TutorialTitle"),
                text = scene.find_entity("TutorialText"), arrow = scene.find_entity("TutorialArrow"),
                glow = scene.find_entity("TutorialGlow") }
    show_hint(false)
    Tut.pad = S.pads[1]
    if not Tut.offered() then
        UI.show(S.hud.skip_tutorial, false)
        mirror()
        return
    end
    UI.show(S.hud.skip_tutorial, true)
    enter("build")
end

function Tut.skip()
    if Tut.step == "off" and not Tut.offered() then return end
    finish("skipped")
end

-- The pixel the hint points at and the room around it: a pad, or a widget of the HUD as it is laid out (the Call wave
-- button, the phase bar), whatever the interface size.
local function target()
    local width, height = scene.get_viewport_size()
    if width == nil or width <= 0 then width, height = 1280, 720 end
    local widget = Tut.step == "call" and S.hud.call or (Tut.step == "night" and S.hud.phase_bar or nil)
    if widget ~= nil then
        local x, y, w, h = ui.get_rect(widget)
        if x ~= nil then return x + w * 0.5, y + h * 0.5, false, nil, w * 0.5, h * 0.5 end
        return width * 0.5, height * 0.5, false, nil, 0, 0
    end
    local pad = Tut.pad
    if Tut.step == "sell" or Tut.step == "upgrade" then
        pad = (S.towers[1] and S.towers[1].pad) or pad
    end
    if pad == nil then return width * 0.5, height * 0.5, false, nil, 0, 0 end
    local px, py, unit = Cam.project(pad.x, pad.y + 0.4, pad.z)
    local reach = 0.6 / unit    -- half the pad, in pixels
    return px, py, true, pad, reach, reach
end

-- The scroll goes beside its target, on the side with more room, never over it (nor over the wheel of a pad, which
-- opens on the pad itself); the arrow lies between them, pointing at the target.
local function place()
    local width, height = scene.get_viewport_size()
    if width == nil or width <= 0 then width, height = 1280, 720 end
    local tx, ty, in_world, pad, half_w, half_h = target()
    local gap = 10
    local side = tx > width * 0.5 and -1.0 or 1.0
    local hx = tx + side * (half_w + gap + ARROW + PLAQUE_W * 0.5)
    local hy = ty
    if not in_world and (hx - PLAQUE_W * 0.5 < 8 or hx + PLAQUE_W * 0.5 > width - 8) then
        -- No room beside a widget at the edge: the scroll goes above or below it.
        hx = tx
        hy = ty + (ty > height * 0.5 and -1.0 or 1.0) * (half_h + gap + ARROW + PLAQUE_H * 0.5)
    end
    hx = math.max(PLAQUE_W * 0.5 + 8, math.min(width - PLAQUE_W * 0.5 - 8, hx))
    hy = math.max(height * 0.2 + PLAQUE_H * 0.5, math.min(height * 0.88 - PLAQUE_H * 0.5, hy))
    local rx, ry, rz = S.view.x, C.GRASS_TOP, S.view.z
    local px, py, unit = Cam.project(rx, ry, rz)
    local function at(x, y, behind)
        return Cam.screen_offset(rx, ry, rz, x - px, py - y, unit, behind)
    end
    transform.set_position(Tut.ids.plaque, at(hx, hy, 0.2))
    transform.set_scale(Tut.ids.plaque, PLAQUE_W * unit, PLAQUE_H * unit, 1.0)
    transform.set_position(Tut.ids.title, at(hx, hy - PLAQUE_H * 0.5 + 34, -0.1))
    transform.set_scale(Tut.ids.title, 26 * unit, 26 * unit, 1.0)
    transform.set_position(Tut.ids.text, at(hx, hy - PLAQUE_H * 0.5 + 68, -0.1))
    transform.set_scale(Tut.ids.text, 18 * unit, 18 * unit, 1.0)
    -- The arrow points from the scroll to the target and stops at the edge of the target, bobbing toward it.
    local dx, dy = tx - hx, ty - hy
    local length = math.max(1.0, math.sqrt(dx * dx + dy * dy))
    local ux, uy = dx / length, dy / length
    local edge = math.min(half_w / math.max(math.abs(ux), 1e-3), half_h / math.max(math.abs(uy), 1e-3))
    local tip = edge + 8 + 6 * (1.0 + math.sin(Tut.clock * 6.0))
    local ax, ay = tx - ux * (tip + ARROW * 0.5), ty - uy * (tip + ARROW * 0.5)
    transform.set_position(Tut.ids.arrow, at(ax, ay, -0.2))
    transform.set_scale(Tut.ids.arrow, ARROW * unit, ARROW * unit, 1.0)
    transform.set_rotation(Tut.ids.arrow, 0.0, 0.0, math.atan(-dy, dx))
    gamestate.set("tower_tutorial_target_x", math.floor(tx + 0.5))
    gamestate.set("tower_tutorial_target_y", math.floor(ty + 0.5))
    gamestate.set("tower_tutorial_arrow_x", math.floor(ax + 0.5))
    UI.show(Tut.ids.glow, in_world)
    if in_world and pad ~= nil then
        transform.set_position(Tut.ids.glow, pad.x, pad.y + 0.04, pad.z)
        local size = 1.3 + 0.12 * math.sin(Tut.clock * 5.0)
        transform.set_scale(Tut.ids.glow, size, size, 1.0)
    end
end

-- Done when the action of the step happened since it began.
local function reached()
    local base = Tut.baseline
    if Tut.step == "build" then return S.built > base.built end
    if Tut.step == "call" then return S.wave > base.wave end
    if Tut.step == "upgrade" then return S.upgrades > base.upgrades end
    if Tut.step == "sell" then return #S.towers < base.towers end
    if Tut.step == "night" then
        return Tut.clock > 0.3 and (input.is_action_just_pressed("ui_accept") or input.is_action_just_pressed("confirm")
                or input.is_action_just_pressed("primary"))
    end
    return false
end

function Tut.update(real_dt)
    if Tut.step == "off" then return end
    Tut.clock = Tut.clock + real_dt
    -- Nothing to sell yet: the step waits for a tower (bought back if the first one was already sold).
    if Tut.step == "sell" and #S.towers == 0 then Tut.baseline.towers = 1 end
    if Tut.step ~= "call" then time.set_scale(SLOW) end
    if reached() then
        next_step()
        return
    end
    place()
    mirror()
end

function Tut.active()
    return Tut.step ~= "off"
end

return Tut
