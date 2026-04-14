-- Settings menu controller.
-- Reads current settings values on create, updates them via slider callbacks.

local vol_label_id = 0
local speed_label_id = 0

function on_create()
    log.info("Settings menu loaded")

    -- Find label entities for dynamic text updates.
    vol_label_id = scene.find_entity("MasterVolLabel")
    speed_label_id = scene.find_entity("PlayerSpeedLabel")

    -- Initialize labels with current values.
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
end

function on_destroy()
end

-- Slider callback: master volume changed (value 0..1).
function on_master_vol_changed(value)
    settings.set("volume_master", value)
    settings.apply()
    if vol_label_id ~= 0 then
        ui.set_text(vol_label_id, string.format("Master Volume: %d%%", math.floor(value * 100)))
    end
end

-- Slider callback: player speed changed (value 0..1, mapped to 2..20).
function on_speed_changed(value)
    local speed = 2.0 + value * 18.0
    settings.set("player_speed", speed)
    if speed_label_id ~= 0 then
        ui.set_text(speed_label_id, string.format("Player Speed: %.1f", speed))
    end
end

-- Back button: save and return to main menu.
function go_back()
    settings.save()
    log.info("Settings saved")
    scene.load_scene("scenes/main_menu.owl")
end
