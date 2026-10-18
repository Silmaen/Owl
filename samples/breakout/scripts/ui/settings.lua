-- Settings of Breakout, copied from the Engine Demo settings menu (scripts/ui/settings_menu.lua): master volume,
-- mouse speed of the paddle and interface size; Back (or Esc, pad B) saves them and returns to the title.
-- Music and effects share the master volume: a looping SoundSource has no volume of its own yet (gap G9).

local vol_label_id = 0
local vol_slider_id = 0
local mouse_label_id = 0
local mouse_slider_id = 0
local ui_scale_label_id = 0
local ui_scale_slider_id = 0
local leaving = false

local UI_SCALE_MIN, UI_SCALE_MAX = 0.75, 2.0
local MOUSE_MIN, MOUSE_MAX = 0.25, 3.0

local function update_labels()
    if vol_label_id ~= 0 then
        local vol = settings.get("volume_master", 1.0)
        ui.set_text(vol_label_id, string.format("Master Volume: %d%%", math.floor(vol * 100 + 0.5)))
    end
    if mouse_label_id ~= 0 then
        local speed = settings.get("breakout_mouse_speed", 1.0)
        ui.set_text(mouse_label_id, string.format("Mouse Speed: %d%%", math.floor(speed * 100 + 0.5)))
    end
    if ui_scale_label_id ~= 0 then
        local scale = settings.get("ui_scale", 1.0)
        ui.set_text(ui_scale_label_id, string.format("Interface Size: %d%%", math.floor(scale * 100 + 0.5)))
    end
end

local function update_sliders()
    if vol_slider_id ~= 0 then
        ui.set_slider_value(vol_slider_id, settings.get("volume_master", 1.0))
    end
    if mouse_slider_id ~= 0 then
        local speed = settings.get("breakout_mouse_speed", 1.0)
        ui.set_slider_value(mouse_slider_id, (speed - MOUSE_MIN) / (MOUSE_MAX - MOUSE_MIN))
    end
    if ui_scale_slider_id ~= 0 then
        local scale = settings.get("ui_scale", 1.0)
        ui.set_slider_value(ui_scale_slider_id, (scale - UI_SCALE_MIN) / (UI_SCALE_MAX - UI_SCALE_MIN))
    end
end

function on_create()
    ui.transition_fade_in(0.5)
    vol_label_id = scene.find_entity("MasterVolLabel")
    vol_slider_id = scene.find_entity("MasterVolSlider")
    mouse_label_id = scene.find_entity("MouseSpeedLabel")
    mouse_slider_id = scene.find_entity("MouseSpeedSlider")
    ui_scale_label_id = scene.find_entity("UiScaleLabel")
    ui_scale_slider_id = scene.find_entity("UiScaleSlider")
    update_sliders()
    update_labels()
end

function on_update(dt)
    if not leaving and input.is_action_just_pressed("ui_cancel") then
        go_back()
    end
end

function on_slider_focused()
end

function on_master_vol_changed()
    settings.set("volume_master", _slider_value or 1.0)
    settings.apply()
    update_labels()
end

function on_mouse_speed_changed()
    local value = _slider_value or 0.27
    settings.set("breakout_mouse_speed", MOUSE_MIN + value * (MOUSE_MAX - MOUSE_MIN))
    update_labels()
end

function on_ui_scale_changed()
    local value = _slider_value or 0.2
    settings.set("ui_scale", UI_SCALE_MIN + value * (UI_SCALE_MAX - UI_SCALE_MIN))
    update_labels()
end

-- Reset: every setting back to game_settings.yml, the best score kept.
function on_reset_clicked()
    local best = settings.get("breakout_best", 0)
    settings.reset_all()
    settings.set("breakout_best", best)
    settings.apply()
    update_sliders()
    update_labels()
end

function go_back()
    if leaving then return end
    leaving = true
    settings.save()
    scene.transition_to("scenes/title.owl", "fade_out", 0.3)
end
