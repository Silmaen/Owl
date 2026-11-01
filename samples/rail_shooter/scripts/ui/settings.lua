-- Settings of Ring Runner: the master, music and effects volumes (the `volume_*` settings the mixer buses follow),
-- the interface size and the inverted steering; Back (or Esc, pad B) saves them and returns to the title.

properties = {
    { name = "ui_scale_min", type = "float", default = 0.75, min = 0.1 },
    { name = "ui_scale_max", type = "float", default = 2.0, min = 0.1 },
}

local VOLUMES = {
    { setting = "volume_master", slider = "MasterVolSlider", label = "MasterVolLabel", text = "MASTER VOLUME" },
    { setting = "volume_music", slider = "MusicVolSlider", label = "MusicVolLabel", text = "MUSIC" },
    { setting = "volume_sfx", slider = "SfxVolSlider", label = "SfxVolLabel", text = "EFFECTS" },
}

local ids = {}
local leaving = false

local function percent(value)
    return math.floor(value * 100 + 0.5)
end

local function refresh()
    for _, row in ipairs(VOLUMES) do
        local value = settings.get(row.setting, 1.0)
        if ids[row.slider] ~= 0 then ui.set_slider_value(ids[row.slider], value) end
        if ids[row.label] ~= 0 then ui.set_text(ids[row.label], string.format("%s %d%%", row.text, percent(value))) end
    end
    local scale = settings.get("ui_scale", 1.0)
    if ids.UiScaleSlider ~= 0 then
        ui.set_slider_value(ids.UiScaleSlider, (scale - ui_scale_min) / (ui_scale_max - ui_scale_min))
    end
    if ids.UiScaleLabel ~= 0 then ui.set_text(ids.UiScaleLabel, string.format("INTERFACE SIZE %d%%", percent(scale))) end
    local inverted = settings.get("rail_invert_y", false)
    if ids.InvertButton ~= 0 then
        ui.set_text(ids.InvertButton, inverted and "INVERT STEERING: ON" or "INVERT STEERING: OFF")
    end
    gamestate.set("rail_invert_y", inverted)
end

function on_create()
    if not ui.is_transition_active() then ui.transition_play("wipe_left", 0.5, 0.0, 0.05, 0.14) end
    for _, tag in ipairs({ "MasterVolSlider", "MasterVolLabel", "MusicVolSlider", "MusicVolLabel", "SfxVolSlider",
                           "SfxVolLabel", "UiScaleSlider", "UiScaleLabel", "InvertButton" }) do
        ids[tag] = scene.find_entity(tag)
    end
    refresh()
end

function on_update(dt)
    if not leaving and input.is_action_just_pressed("ui_cancel") then go_back() end
end

function on_slider_focused()
end

local function set_volume(setting)
    settings.set(setting, _slider_value or 1.0)
    settings.apply()
    refresh()
end

function on_master_vol_changed()
    set_volume("volume_master")
end

function on_music_vol_changed()
    set_volume("volume_music")
end

function on_sfx_vol_changed()
    set_volume("volume_sfx")
    sound.play("sounds/ui/select.wav")
end

function on_ui_scale_changed()
    settings.set("ui_scale", ui_scale_min + (_slider_value or 0.2) * (ui_scale_max - ui_scale_min))
    refresh()
end

function on_invert_clicked()
    settings.set("rail_invert_y", not settings.get("rail_invert_y", false))
    sound.play("sounds/ui/select.wav")
    refresh()
end

-- Reset: volumes, interface size and steering back to game_settings.yml, the best score kept.
function on_reset_clicked()
    for _, key in ipairs({ "volume_master", "volume_music", "volume_sfx", "ui_scale", "rail_invert_y" }) do
        settings.reset(key)
    end
    settings.apply()
    refresh()
end

function go_back()
    if leaving then return end
    leaving = true
    settings.save()
    scene.transition_to("scenes/title.owl", "wipe_left", 0.45, 0.0, 0.05, 0.14)
end
