-- Ending of The Moon Lantern: the sequence `sequences/ending.owlseq` closes in on the lit lantern and calls the events
-- below to tell the end in three lines; then "The End", Barnaby dances and a button goes back to the title. A click,
-- Enter or pad A skips to the end.

properties = {
    { name = "glow_time", type = "float", default = 2.0, min = 0.1 },
}

local ids = {}
local glow = nil
local done = false

function on_create()
    for _, name in ipairs({ "MainCamera", "Glow", "EndLine", "EndText", "TheEnd", "TitleButton", "Barnaby", "Hero" }) do
        ids[name] = scene.find_entity(name)
    end
    if ids.EndLine == 0 then ids.EndLine = ui.find("EndLine") end
    gamestate.set("ending_line", 0)
    if not ui.is_transition_active() then ui.transition_play("fade_in", 1.2, 0.85, 0.9, 1.0) end
end

local function show_line(index)
    ui.set_visible(ids.EndLine, true)
    ui.set_text(ids.EndText, tr("ending.line" .. index))
    gamestate.set("ending_line", index)
end

function on_ending_glow() glow = 0 end
function on_ending_line_1() show_line(1) end
function on_ending_line_2() show_line(2) end
function on_ending_line_3()
    show_line(3)
    animator.trigger(ids.Hero, "talk")
end
function on_ending_dance() animator.trigger(ids.Barnaby, "dance") end

function on_ending_done()
    if done then return end
    done = true
    ui.set_visible(ids.EndLine, false)
    ui.set_visible(ids.TheEnd, true)
    ui.set_visible(ids.TitleButton, true)
    ui.set_focus(ids.TitleButton)
    gamestate.set("ending_done", true)
end

function on_update(dt)
    if glow ~= nil and glow < glow_time then
        glow = math.min(glow_time, glow + dt)
        entity.set_tint(ids.Glow, 0.85, 0.92, 1.0, 0.7 * glow / glow_time)
    end
    if not done and (input.is_action_just_pressed("primary") or input.is_action_just_pressed("act")) then
        sequence.stop(ids.MainCamera)
        on_ending_dance()
        on_ending_done()
    end
end

function on_title_clicked()
    sound.play("sounds/click.wav")
    gamestate.clear()
    scene.transition_to("scenes/title.owl", "fade_out", 0.8, 0.02, 0.02, 0.05)
end
