-- Settings of The Moon Lantern: the master, music and effects volumes, the language (English or French) and the
-- subtitles of the sounds; Back (or `ui_cancel`: Escape, pad B) saves them to `settings.yml`.

local ids = {}
local leaving = false

local function percent(key)
    return math.floor(settings.get(key, 1.0) * 100 + 0.5)
end

local function update_labels()
    ui.set_text(ids.MasterLabel, string.format(tr("settings.master"), percent("volume_master")))
    ui.set_text(ids.MusicLabel, string.format(tr("settings.music"), percent("volume_music")))
    ui.set_text(ids.EffectsLabel, string.format(tr("settings.effects"), percent("volume_sfx")))
    ui.set_text(ids.LanguageButton, tr("settings.language"))
    ui.set_text(ids.SubtitlesButton, tr(settings.get("subtitles", true) and "settings.subtitles_on" or "settings.subtitles_off"))
end

local function update_sliders()
    ui.set_slider_value(ids.MasterSlider, settings.get("volume_master", 1.0))
    ui.set_slider_value(ids.MusicSlider, settings.get("volume_music", 0.8))
    ui.set_slider_value(ids.EffectsSlider, settings.get("volume_sfx", 1.0))
end

function on_create()
    for _, name in ipairs({ "MasterLabel", "MasterSlider", "MusicLabel", "MusicSlider", "EffectsLabel",
                            "EffectsSlider", "LanguageButton", "SubtitlesButton" }) do
        ids[name] = scene.find_entity(name)
    end
    update_sliders()
    update_labels()
    if not ui.is_transition_active() then ui.transition_play("fade_in", 0.4, 0.02, 0.02, 0.05) end
end

function on_update(dt)
    if not leaving and input.is_action_just_pressed("ui_cancel") then go_back() end
end

local function set_volume(key)
    settings.set(key, _slider_value or 1.0)
    settings.apply()
    update_labels()
end

function on_master_changed() set_volume("volume_master") end
function on_music_changed() set_volume("volume_music") end
function on_effects_changed() set_volume("volume_sfx") end

function on_language_clicked()
    local next_language = locale.get_language() == "fr" and "en" or "fr"
    settings.set("language", next_language)
    settings.apply()
    sound.play("sounds/click.wav")
    update_labels()
end

function on_subtitles_clicked()
    settings.set("subtitles", not settings.get("subtitles", true))
    settings.apply()
    sound.play("sounds/click.wav")
    update_labels()
end

function on_reset_clicked()
    for _, key in ipairs({ "volume_master", "volume_music", "volume_sfx", "language", "subtitles" }) do
        settings.reset(key)
    end
    settings.apply()
    update_sliders()
    update_labels()
end

function go_back()
    if leaving then return end
    leaving = true
    settings.save()
    scene.transition_to("scenes/title.owl", "fade_out", 0.3, 0.02, 0.02, 0.05)
end
