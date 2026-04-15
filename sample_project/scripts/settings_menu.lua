-- Settings menu controller.
-- Demonstrates: settings.get/set/save/load/apply/reset/reset_all,
--               ui.set_text, ui.set_slider_value, ui.get_slider_value,
--               ui.transition_fade_in/out, scene.load_scene

local vol_label_id = 0
local speed_label_id = 0
local vol_slider_id = 0
local speed_slider_id = 0
local pending_scene = nil

function on_create()
    log.info("Settings menu loaded")
    ui.transition_fade_in(0.5)

    -- Find UI entities.
    vol_label_id = scene.find_entity("MasterVolLabel")
    speed_label_id = scene.find_entity("PlayerSpeedLabel")
    vol_slider_id = scene.find_entity("MasterVolSlider")
    speed_slider_id = scene.find_entity("PlayerSpeedSlider")

    -- Initialize slider positions from current settings values.
    local vol = settings.get("volume_master", 1.0)
    local speed = settings.get("player_speed", 8.0)

    if vol_slider_id ~= 0 then
        ui.set_slider_value(vol_slider_id, vol)
    end
    if speed_slider_id ~= 0 then
        -- Speed range 2..20 mapped to 0..1
        ui.set_slider_value(speed_slider_id, (speed - 2.0) / 18.0)
    end

    update_labels()
end

function update_labels()
    local vol = settings.get("volume_master", 1.0)
    local speed = settings.get("player_speed", 8.0)

    if vol_label_id ~= 0 then
        ui.set_text(vol_label_id, string.format("Master Volume: %d%%", math.floor(vol * 100)))
    end
    if speed_label_id ~= 0 then
        ui.set_text(speed_label_id, string.format("Player Speed: %.1f", speed))
    end
end

function on_update(dt)
    if pending_scene then
        if not ui.is_transition_active() then
            scene.load_scene(pending_scene)
            pending_scene = nil
        end
    end
end

function on_destroy()
end

-- Slider callback: master volume changed (value 0..1).
function on_master_vol_changed()
    local value = _slider_value or 1.0
    settings.set("volume_master", value)
    settings.apply()
    if vol_label_id ~= 0 then
        ui.set_text(vol_label_id, string.format("Master Volume: %d%%", math.floor(value * 100)))
    end
end

-- Slider callback: player speed changed (value 0..1, mapped to 2..20).
function on_speed_changed()
    local value = _slider_value or 0.5
    local speed = 2.0 + value * 18.0
    settings.set("player_speed", speed)
    if speed_label_id ~= 0 then
        ui.set_text(speed_label_id, string.format("Player Speed: %.1f", speed))
    end
end

-- Reset button: restore all settings to game_settings.yml defaults.
function on_reset_clicked()
    log.info("Resetting all settings to defaults")
    -- Reset individual keys first (demonstrates settings.reset per-key)
    settings.reset("volume_master")
    settings.reset("player_speed")
    -- Then full reset for any remaining keys
    settings.reset_all()
    settings.load()
    settings.apply()

    -- Re-read values and update sliders + labels.
    local vol = settings.get("volume_master", 1.0)
    local speed = settings.get("player_speed", 8.0)

    if vol_slider_id ~= 0 then
        ui.set_slider_value(vol_slider_id, vol)
    end
    if speed_slider_id ~= 0 then
        ui.set_slider_value(speed_slider_id, (speed - 2.0) / 18.0)
    end

    update_labels()
end

-- Back button: save and return to main menu.
function go_back()
    settings.save()
    log.info("Settings saved")
    pending_scene = "scenes/main_menu.owl"
    ui.transition_fade_out(0.3)
end
