-- Settings of Sokoban, after the Engine Demo settings menu (scripts/ui/settings_menu.lua): master volume and interface
-- size; Reset progress forgets the levels done and the best moves; Back (or Esc, pad B) saves and returns to the title.
-- Music and effects share the master volume: a looping SoundSource has no volume of its own yet (gap G9).

local LEVEL_COUNT = 10
local UI_SCALE_MIN, UI_SCALE_MAX = 0.75, 2.0

local vol_label_id, vol_slider_id = 0, 0
local ui_scale_label_id, ui_scale_slider_id = 0, 0
local progress_id = 0
local leaving = false

local function levels_done()
    local done = 0
    for i = 1, LEVEL_COUNT do
        if settings.get(string.format("sokoban_done_%02d", i), false) then done = done + 1 end
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
        ui.set_text(progress_id, "Levels done: " .. levels_done() .. " / " .. LEVEL_COUNT)
    end
end

local function update_sliders()
    if vol_slider_id ~= 0 then
        ui.set_slider_value(vol_slider_id, settings.get("volume_master", 1.0))
    end
    if ui_scale_slider_id ~= 0 then
        local scale = settings.get("ui_scale", 1.0)
        ui.set_slider_value(ui_scale_slider_id, (scale - UI_SCALE_MIN) / (UI_SCALE_MAX - UI_SCALE_MIN))
    end
end

function on_create()
    if not ui.is_transition_active() then ui.transition_play("wipe_left", 0.4, 0.2, 0.12, 0.05) end
    vol_label_id = scene.find_entity("MasterVolLabel")
    vol_slider_id = scene.find_entity("MasterVolSlider")
    ui_scale_label_id = scene.find_entity("UiScaleLabel")
    ui_scale_slider_id = scene.find_entity("UiScaleSlider")
    progress_id = scene.find_entity("ProgressLabel")
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

function on_ui_scale_changed()
    local value = _slider_value or 0.2
    settings.set("ui_scale", UI_SCALE_MIN + value * (UI_SCALE_MAX - UI_SCALE_MIN))
    update_labels()
end

-- Reset progress: every level back to not done, the best moves forgotten.
function on_reset_progress_clicked()
    for i = 1, LEVEL_COUNT do
        settings.reset(string.format("sokoban_done_%02d", i))
        settings.reset(string.format("sokoban_best_%02d", i))
    end
    settings.save()
    update_labels()
end

-- Reset defaults: volume and interface size back to game_settings.yml, the progress kept.
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
    scene.transition_to("scenes/title.owl", "wipe_left", 0.3, 0.2, 0.12, 0.05)
end
