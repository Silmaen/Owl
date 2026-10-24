-- Settings of Tower defence: master volume and interface size; Reset progress forgets the levels, stars and
-- upgrades, Reset tutorial offers the tutorial of the first level again; Back (or Esc, pad B) saves and returns to the
-- title. Effects share the master volume (gap G9).

properties = {
    { name = "ui_scale_min", type = "float", default = 0.75, min = 0.1 },
    { name = "ui_scale_max", type = "float", default = 2.0, min = 0.1 },
    { name = "map_count", type = "int", default = 10, min = 1 },
    { name = "wipe_color", type = "color", default = { 0.1, 0.14, 0.24, 1 } },
}

local vol_label_id, vol_slider_id = 0, 0
local ui_scale_label_id, ui_scale_slider_id = 0, 0
local progress_id = 0
local leaving = false

local function maps_done()
    local done = 0
    for i = 1, map_count do
        if settings.get(string.format("tower_done_%02d", i), false) then done = done + 1 end
    end
    return done
end

local function update_labels()
    if vol_label_id ~= 0 then
        local vol = settings.get("volume_master", 1.0)
        ui.set_text(vol_label_id, string.format("Master Volume: %d%%", math.floor(vol * 100 + 0.5)))
    end
    if ui_scale_label_id ~= 0 then
        local scale = settings.get("ui_scale", 1.0)
        ui.set_text(ui_scale_label_id, string.format("Interface Size: %d%%", math.floor(scale * 100 + 0.5)))
    end
    if progress_id ~= 0 then
        ui.set_text(progress_id, "Maps defended: " .. maps_done() .. " / " .. map_count)
    end
end

local function update_sliders()
    if vol_slider_id ~= 0 then ui.set_slider_value(vol_slider_id, settings.get("volume_master", 1.0)) end
    if ui_scale_slider_id ~= 0 then
        local scale = settings.get("ui_scale", 1.0)
        ui.set_slider_value(ui_scale_slider_id, (scale - ui_scale_min) / (ui_scale_max - ui_scale_min))
    end
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("wipe_down", 0.4, wipe_color.r, wipe_color.g, wipe_color.b)
    end
    vol_label_id = scene.find_entity("MasterVolLabel")
    vol_slider_id = scene.find_entity("MasterVolSlider")
    ui_scale_label_id = scene.find_entity("UiScaleLabel")
    ui_scale_slider_id = scene.find_entity("UiScaleSlider")
    progress_id = scene.find_entity("ProgressLabel")
    update_sliders()
    update_labels()
end

function on_update(dt)
    if not leaving and input.is_action_just_pressed("ui_cancel") then go_back() end
end

function on_slider_focused()
end

function on_master_vol_changed()
    settings.set("volume_master", _slider_value or 1.0)
    settings.apply()
    update_labels()
end

function on_ui_scale_changed()
    local value = _slider_value or 0.2
    settings.set("ui_scale", ui_scale_min + value * (ui_scale_max - ui_scale_min))
    update_labels()
end

function on_reset_progress_clicked()
    for i = 1, map_count do
        settings.reset(string.format("tower_done_%02d", i))
        settings.reset(string.format("tower_best_%02d", i))
        settings.reset(string.format("tower_stars_%02d", i))
    end
    for _, family in ipairs({ "archer", "cannon", "frost", "rain", "shield", "treasury" }) do
        settings.reset("tower_up_" .. family)
    end
    settings.save()
    update_labels()
end

-- The tutorial of the first level is offered again.
function on_reset_tutorial_clicked()
    settings.reset("tower_tutorial")
    settings.save()
    sound.play("sounds/ui/click.wav")
    gamestate.set("tower_tutorial_reset", true)
end

function on_reset_clicked()
    settings.reset("volume_master")
    settings.reset("ui_scale")
    settings.apply()
    update_sliders()
    update_labels()
end

function go_back()
    if leaving then return end
    leaving = true
    settings.save()
    scene.transition_to("scenes/title.owl", "wipe_down", 0.3, wipe_color.r, wipe_color.g, wipe_color.b)
end
