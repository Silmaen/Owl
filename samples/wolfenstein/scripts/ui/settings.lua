-- Settings screen of Wolfenstein mini (from the Engine Demo's `scripts/ui/settings_menu.lua`): the master, music and
-- effects volumes (the `volume_*` settings the mixer buses follow; the effects slider sets the SFX and Ambient buses,
-- the ambience of the floors being an effect to the player), the mouse turn sensitivity, the interface size and the
-- accessibility settings (colour vision correction, reduced motion, subtitles). Back (or `ui_cancel`: Escape, pad B)
-- saves them to `settings.yml`.

properties = {
    { name = "ui_scale_min", type = "float", default = 0.75, min = 0.1 },
    { name = "ui_scale_max", type = "float", default = 2.0, min = 0.1 },
    { name = "sensitivity_min", type = "float", default = 0.25, min = 0.01 },
    { name = "sensitivity_max", type = "float", default = 3.0, min = 0.01 },
}

local VOLUMES = {
    { setting = "volume_master", slider = "MasterVolSlider", label = "MasterVolLabel", text = "Master volume" },
    { setting = "volume_music", slider = "MusicVolSlider", label = "MusicVolLabel", text = "Music" },
    { setting = "volume_sfx", slider = "SfxVolSlider", label = "SfxVolLabel", text = "Effects" },
}
-- The colour vision corrections of the `color_blind_mode` setting, in the order the button cycles them.
local COLOR_BLIND_MODES = { "none", "protanopia", "deuteranopia", "tritanopia" }
local COLOR_BLIND_NAMES = { none = "STANDARD", protanopia = "PROTANOPIA", deuteranopia = "DEUTERANOPIA",
                            tritanopia = "TRITANOPIA" }
local RESET_KEYS = { "volume_master", "volume_music", "volume_sfx", "volume_ambient", "mouse_sensitivity", "ui_scale",
                     "color_blind_mode", "reduced_motion", "subtitles" }

local ids = {}
local leaving = false

local function percent(value)
    return math.floor(value * 100 + 0.5)
end

local function on_off(value)
    return value and "ON" or "OFF"
end

local function update_labels()
    for _, row in ipairs(VOLUMES) do
        ui.set_text(ids[row.label], string.format("%s: %d%%", row.text, percent(settings.get(row.setting, 1.0))))
    end
    ui.set_text(ids.MouseLabel, string.format("Mouse turn: %.2f", settings.get("mouse_sensitivity", 1.0)))
    ui.set_text(ids.UiScaleLabel, string.format("Interface size: %d%%", percent(settings.get("ui_scale", 1.0))))
    local mode = settings.get("color_blind_mode", "none")
    ui.set_text(ids.ColorBlindButton, "COLOUR VISION: " .. (COLOR_BLIND_NAMES[mode] or string.upper(mode)))
    ui.set_text(ids.ReducedMotionButton, "REDUCED MOTION: " .. on_off(settings.get("reduced_motion", false)))
    ui.set_text(ids.SubtitlesButton, "SUBTITLES: " .. on_off(settings.get("subtitles", true)))
end

local function update_sliders()
    for _, row in ipairs(VOLUMES) do
        ui.set_slider_value(ids[row.slider], settings.get(row.setting, 1.0))
    end
    ui.set_slider_value(ids.MouseSlider, (settings.get("mouse_sensitivity", 1.0) - sensitivity_min) / (sensitivity_max - sensitivity_min))
    ui.set_slider_value(ids.UiScaleSlider, (settings.get("ui_scale", 1.0) - ui_scale_min) / (ui_scale_max - ui_scale_min))
end

function on_create()
    if not ui.is_transition_active() then ui.transition_play("fade_in", 0.5, 0.22, 0.02, 0.02) end
    for _, name in ipairs({ "MasterVolLabel", "MasterVolSlider", "MusicVolLabel", "MusicVolSlider", "SfxVolLabel",
                            "SfxVolSlider", "MouseLabel", "MouseSlider", "UiScaleLabel", "UiScaleSlider",
                            "ColorBlindButton", "ReducedMotionButton", "SubtitlesButton" }) do
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

local function set_volume(setting)
    settings.set(setting, _slider_value or 1.0)
    settings.apply()
    update_labels()
end

function on_master_vol_changed()
    set_volume("volume_master")
end

function on_music_vol_changed()
    set_volume("volume_music")
end

function on_sfx_vol_changed()
    settings.set("volume_ambient", _slider_value or 1.0)
    set_volume("volume_sfx")
    sound.play("sounds/game/key.wav")
end

function on_mouse_changed()
    settings.set("mouse_sensitivity", sensitivity_min + (_slider_value or 0.3) * (sensitivity_max - sensitivity_min))
    update_labels()
end

function on_ui_scale_changed()
    settings.set("ui_scale", ui_scale_min + (_slider_value or 0.2) * (ui_scale_max - ui_scale_min))
    update_labels()
end

-- Colour vision button: the next correction, applied to the whole frame at once.
function on_color_blind_clicked()
    local current = settings.get("color_blind_mode", "none")
    local next_mode = COLOR_BLIND_MODES[1]
    for index, mode in ipairs(COLOR_BLIND_MODES) do
        if mode == current then
            next_mode = COLOR_BLIND_MODES[index % #COLOR_BLIND_MODES + 1]
            break
        end
    end
    settings.set("color_blind_mode", next_mode)
    update_labels()
end

-- Reduced motion button: the fades instead of the wipes, the title view still, no turn in the elevator ride.
function on_reduced_motion_clicked()
    settings.set("reduced_motion", not settings.get("reduced_motion", false))
    settings.apply()
    update_labels()
end

-- Subtitles button: a sample line shows them when they are turned on.
function on_subtitles_clicked()
    local shown = not settings.get("subtitles", true)
    settings.set("subtitles", shown)
    if shown then
        ui.show_subtitle("[Stone grinds: the wall slides away]", 3)
    else
        ui.clear_subtitles()
    end
    update_labels()
end

-- Reset: every setting of this screen back to game_settings.yml, the service record kept.
function on_reset_clicked()
    for _, key in ipairs(RESET_KEYS) do
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
