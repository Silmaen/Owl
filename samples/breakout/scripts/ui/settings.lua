-- Settings of Breakout: the master, music and effects volumes (the `volume_*` settings the mixer buses follow), the
-- mouse speed of the paddle and the interface size; Back (or Esc, pad B) saves them and returns to the title (a magenta
-- wipe, like every Breakout screen).

properties = {
    { name = "ui_scale_min", type = "float", default = 0.75, min = 0.1 },
    { name = "ui_scale_max", type = "float", default = 2.0, min = 0.1 },
    { name = "mouse_min", type = "float", default = 0.25, min = 0.01 },
    { name = "mouse_max", type = "float", default = 3.0, min = 0.01 },
}

local VOLUMES = {
    { setting = "volume_master", slider = "MasterVolSlider", label = "MasterVolLabel", text = "Master Volume" },
    { setting = "volume_music", slider = "MusicVolSlider", label = "MusicVolLabel", text = "Music" },
    { setting = "volume_sfx", slider = "SfxVolSlider", label = "SfxVolLabel", text = "Effects" },
}

local ids = {}
local leaving = false

local function percent(value)
    return math.floor(value * 100 + 0.5)
end

local function set_text(tag, text)
    if ids[tag] ~= 0 then ui.set_text(ids[tag], text) end
end

local function set_slider(tag, value)
    if ids[tag] ~= 0 then ui.set_slider_value(ids[tag], value) end
end

local function refresh()
    for _, row in ipairs(VOLUMES) do
        local value = settings.get(row.setting, 1.0)
        set_slider(row.slider, value)
        set_text(row.label, string.format("%s: %d%%", row.text, percent(value)))
    end
    local speed = settings.get("breakout_mouse_speed", 1.0)
    set_slider("MouseSpeedSlider", (speed - mouse_min) / (mouse_max - mouse_min))
    set_text("MouseSpeedLabel", string.format("Mouse Speed: %d%%", percent(speed)))
    local scale = settings.get("ui_scale", 1.0)
    set_slider("UiScaleSlider", (scale - ui_scale_min) / (ui_scale_max - ui_scale_min))
    set_text("UiScaleLabel", string.format("Interface Size: %d%%", percent(scale)))
end

function on_create()
    if not ui.is_transition_active() then ui.transition_play("wipe_up", 0.4, 0.45, 0.05, 0.55) end
    for _, tag in ipairs({ "MasterVolSlider", "MasterVolLabel", "MusicVolSlider", "MusicVolLabel", "SfxVolSlider",
                           "SfxVolLabel", "MouseSpeedSlider", "MouseSpeedLabel", "UiScaleSlider", "UiScaleLabel" }) do
        ids[tag] = scene.find_entity(tag)
    end
    refresh()
end

function on_update(dt)
    if not leaving and input.is_action_just_pressed("ui_cancel") then
        go_back()
    end
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

-- A paddle blip at the new level of the effects.
function on_sfx_vol_changed()
    set_volume("volume_sfx")
    sound.play("sounds/game/paddle.wav")
end

function on_mouse_speed_changed()
    settings.set("breakout_mouse_speed", mouse_min + (_slider_value or 0.27) * (mouse_max - mouse_min))
    refresh()
end

function on_ui_scale_changed()
    settings.set("ui_scale", ui_scale_min + (_slider_value or 0.2) * (ui_scale_max - ui_scale_min))
    refresh()
end

-- Reset: volumes, mouse speed and interface size back to game_settings.yml, the scores and the levels cleared kept.
function on_reset_clicked()
    for _, key in ipairs({ "volume_master", "volume_music", "volume_sfx", "breakout_mouse_speed", "ui_scale" }) do
        settings.reset(key)
    end
    settings.apply()
    refresh()
end

function go_back()
    if leaving then return end
    leaving = true
    settings.save()
    scene.transition_to("scenes/title.owl", "wipe_up", 0.3, 0.45, 0.05, 0.55)
end
