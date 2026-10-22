-- Settings screen of Wolfenstein mini (from the Engine Demo's `scripts/ui/settings_menu.lua`): master volume, mouse
-- turn sensitivity and interface size; Back (or `ui_cancel`: Escape, pad B) saves them to `settings.yml`.

properties = {
    { name = "ui_scale_min", type = "float", default = 0.75, min = 0.1 },
    { name = "ui_scale_max", type = "float", default = 2.0, min = 0.1 },
    { name = "sensitivity_min", type = "float", default = 0.25, min = 0.01 },
    { name = "sensitivity_max", type = "float", default = 3.0, min = 0.01 },
}

local ids = {}
local leaving = false

local function update_labels()
    ui.set_text(ids.MasterVolLabel, string.format("Master volume: %d%%", math.floor(settings.get("volume_master", 1.0) * 100 + 0.5)))
    ui.set_text(ids.MouseLabel, string.format("Mouse turn: %.2f", settings.get("mouse_sensitivity", 1.0)))
    ui.set_text(ids.UiScaleLabel, string.format("Interface size: %d%%", math.floor(settings.get("ui_scale", 1.0) * 100 + 0.5)))
end

local function update_sliders()
    ui.set_slider_value(ids.MasterVolSlider, settings.get("volume_master", 1.0))
    ui.set_slider_value(ids.MouseSlider, (settings.get("mouse_sensitivity", 1.0) - sensitivity_min) / (sensitivity_max - sensitivity_min))
    ui.set_slider_value(ids.UiScaleSlider, (settings.get("ui_scale", 1.0) - ui_scale_min) / (ui_scale_max - ui_scale_min))
end

function on_create()
    if not ui.is_transition_active() then ui.transition_play("fade_in", 0.5, 0.22, 0.02, 0.02) end
    for _, name in ipairs({ "MasterVolLabel", "MasterVolSlider", "MouseLabel", "MouseSlider", "UiScaleLabel",
                            "UiScaleSlider" }) do
        ids[name] = scene.find_entity(name)
    end
    update_sliders()
    update_labels()
end

function on_update(dt)
    if not leaving and input.is_action_just_pressed("ui_cancel") then
        go_back()
    end
end

function on_master_vol_changed()
    settings.set("volume_master", _slider_value or 1.0)
    settings.apply()
    update_labels()
end

function on_mouse_changed()
    settings.set("mouse_sensitivity", sensitivity_min + (_slider_value or 0.3) * (sensitivity_max - sensitivity_min))
    update_labels()
end

function on_ui_scale_changed()
    settings.set("ui_scale", ui_scale_min + (_slider_value or 0.2) * (ui_scale_max - ui_scale_min))
    update_labels()
end

function on_reset_clicked()
    for _, key in ipairs({ "volume_master", "mouse_sensitivity", "ui_scale" }) do
        settings.reset(key)
    end
    settings.apply()
    update_sliders()
    update_labels()
end

function go_back()
    if leaving then
        return
    end
    leaving = true
    settings.save()
    scene.transition_to("scenes/title.owl", "fade_out", 0.4, 0.22, 0.02, 0.02)
end
