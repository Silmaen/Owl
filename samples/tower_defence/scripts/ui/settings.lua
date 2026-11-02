-- Settings of Tower defence: the master, music and effects volumes (the `volume_*` settings the mixer buses follow),
-- the interface size, the accessibility (colour vision correction, reduced motion, subtitles); Reset progress forgets
-- the levels, stars and upgrades, Reset tutorial offers the tutorial of the first level again, Reset defaults puts the
-- volumes, the size and the accessibility back; Back (or Esc, pad B) saves and returns to the title.

properties = {
    { name = "ui_scale_min", type = "float", default = 0.75, min = 0.1 },
    { name = "ui_scale_max", type = "float", default = 2.0, min = 0.1 },
    { name = "map_count", type = "int", default = 10, min = 1 },
    { name = "wipe_color", type = "color", default = { 0.1, 0.14, 0.24, 1 } },
}

local VOLUMES = {
    { setting = "volume_master", slider = "MasterVolSlider", label = "MasterVolLabel", text = "Master volume" },
    { setting = "volume_music", slider = "MusicVolSlider", label = "MusicVolLabel", text = "Music" },
    { setting = "volume_sfx", slider = "SfxVolSlider", label = "SfxVolLabel", text = "Effects" },
}

local COLOUR_MODES = { "none", "protanopia", "deuteranopia", "tritanopia" }
local COLOUR_NAMES = { none = "off", protanopia = "red-blind", deuteranopia = "green-blind",
                       tritanopia = "blue-blind" }

local DEFAULTS = { "volume_master", "volume_music", "volume_sfx", "ui_scale", "color_blind_mode", "reduced_motion",
                   "subtitles" }

local ids = {}
local leaving = false

local function percent(value)
    return math.floor(value * 100 + 0.5)
end

local function set(tag, text)
    if ids[tag] ~= nil and ids[tag] ~= 0 then ui.set_text(ids[tag], text) end
end

local function on_off(flag)
    return flag and "on" or "off"
end

local function maps_done()
    local done = 0
    for i = 1, map_count do
        if settings.get(string.format("tower_done_%02d", i), false) then done = done + 1 end
    end
    return done
end

local function refresh()
    for _, row in ipairs(VOLUMES) do
        local value = settings.get(row.setting, 1.0)
        if ids[row.slider] ~= 0 then ui.set_slider_value(ids[row.slider], value) end
        set(row.label, string.format("%s: %d%%", row.text, percent(value)))
    end
    local scale = settings.get("ui_scale", 1.0)
    if ids.UiScaleSlider ~= 0 then
        ui.set_slider_value(ids.UiScaleSlider, (scale - ui_scale_min) / (ui_scale_max - ui_scale_min))
    end
    set("UiScaleLabel", string.format("Interface size: %d%%", percent(scale)))
    local mode = settings.get("color_blind_mode", "none")
    set("ColourVisionButton", "Colour vision: " .. (COLOUR_NAMES[mode] or mode))
    set("ReducedMotionButton", "Reduced motion: " .. on_off(settings.get("reduced_motion", false)))
    set("SubtitlesButton", "Subtitles: " .. on_off(settings.get("subtitles", true)))
    set("ProgressLabel", "Maps defended: " .. maps_done() .. " / " .. map_count)
    gamestate.set("tower_colour_vision", mode)
    gamestate.set("tower_reduced_motion", settings.get("reduced_motion", false) == true)
    gamestate.set("tower_subtitles", settings.get("subtitles", true) == true)
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("wipe_down", 0.4, wipe_color.r, wipe_color.g, wipe_color.b)
    end
    for _, tag in ipairs({ "MasterVolSlider", "MasterVolLabel", "MusicVolSlider", "MusicVolLabel", "SfxVolSlider",
                           "SfxVolLabel", "UiScaleSlider", "UiScaleLabel", "ColourVisionButton",
                           "ReducedMotionButton", "SubtitlesButton", "ProgressLabel" }) do
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
    sound.play("sounds/ui/click.wav")
end

function on_ui_scale_changed()
    local value = _slider_value or 0.2
    settings.set("ui_scale", ui_scale_min + value * (ui_scale_max - ui_scale_min))
    refresh()
end

-- The next colour vision correction, applied to the whole frame at once.
function on_colour_vision_clicked()
    local current = settings.get("color_blind_mode", "none")
    local next_mode = COLOUR_MODES[1]
    for index, mode in ipairs(COLOUR_MODES) do
        if mode == current then
            next_mode = COLOUR_MODES[index % #COLOUR_MODES + 1]
            break
        end
    end
    settings.set("color_blind_mode", next_mode)
    sound.play("sounds/ui/click.wav")
    refresh()
end

-- Reduced motion: the camera turns at once, the hints and the sites of the map do not bob, the wipes are fades.
function on_reduced_motion_clicked()
    settings.set("reduced_motion", not settings.get("reduced_motion", false))
    settings.apply()
    sound.play("sounds/ui/click.wav")
    refresh()
end

-- Subtitles: a sample line shows them when they are turned on.
function on_subtitles_clicked()
    local shown = not settings.get("subtitles", true)
    settings.set("subtitles", shown)
    if shown then
        ui.show_subtitle("[A war horn sounds]", 3)
    else
        ui.clear_subtitles()
    end
    sound.play("sounds/ui/click.wav")
    refresh()
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
    refresh()
end

-- The tutorial of the first level is offered again.
function on_reset_tutorial_clicked()
    settings.reset("tower_tutorial")
    settings.save()
    sound.play("sounds/ui/click.wav")
    gamestate.set("tower_tutorial_reset", true)
end

-- The volumes, the interface size and the accessibility back to their defaults, the progress kept.
function on_reset_clicked()
    for _, key in ipairs(DEFAULTS) do settings.reset(key) end
    settings.apply()
    ui.clear_subtitles()
    refresh()
end

function go_back()
    if leaving then return end
    leaving = true
    settings.save()
    scene.transition_to("scenes/title.owl", "wipe_down", 0.3, wipe_color.r, wipe_color.g, wipe_color.b)
end
