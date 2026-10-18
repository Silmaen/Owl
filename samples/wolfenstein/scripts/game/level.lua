-- Rules of a Wolfenstein mini floor (script of the `LevelLogic` entity): start, HUD, messages and saves.
--
-- A floor starts when `wolf_floor_started` differs from `level`: the counters of the floor are reset, the totals counted
-- (the pick-ups by their tags, the secret walls tagged `Secret`) and the game saved in slot 1 (the autosave the title's
-- Continue falls back to). A loaded save keeps `wolf_floor_started`, so its counters stay as they were saved.
--
-- Game state: `wolf_score` (whole run), `wolf_items` / `wolf_items_total`, `wolf_secrets` / `wolf_secrets_total`,
-- `wolf_key_gold` / `wolf_key_blue` (0 or 1) and `wolf_explored` (explore.lua); `wolf_toast` is a message shown a few
-- seconds. F6 saves in slot 2, F9 loads it; Escape (pad Start) goes back to the title.

properties = {
    { name = "level", type = "int", default = 1 },
    { name = "title", type = "string", default = "Floor 1" },
}

local ITEM_TAGS = { "Cross", "Chalice", "Chest", "Crown", "Food", "DogFood", "GoldKey", "BlueKey" }
local AUTOSAVE_SLOT, QUICK_SLOT = 1, 2
local TOAST_SECONDS = 2.5

local ids = {}
local shown = {}
local toast_left = 0
local autosave = false
local leaving = false

local function set_text(name, text)
    if ids[name] ~= 0 and shown[name] ~= text then
        shown[name] = text
        ui.set_text(ids[name], text)
    end
end

local function ratio(found, total)
    if total == 0 then
        return 100
    end
    return math.floor(100 * found / total)
end

local function start_floor()
    local items = 0
    for _, tag in ipairs(ITEM_TAGS) do
        items = items + #scene.find_entities(tag)
    end
    gamestate.set("wolf_floor_started", level)
    gamestate.set("wolf_score", gamestate.get("wolf_score", 0))
    gamestate.set("wolf_level", level)
    gamestate.set("wolf_items", 0)
    gamestate.set("wolf_items_total", items)
    gamestate.set("wolf_secrets", 0)
    gamestate.set("wolf_secrets_total", #scene.find_entities("Secret"))
    gamestate.set("wolf_explored", 0)
    gamestate.set("wolf_key_gold", 0)
    gamestate.set("wolf_key_blue", 0)
    gamestate.set("wolf_floor_score", gamestate.get("wolf_score", 0))
    gamestate.remove("wolf_seen_map")
    gamestate.set("wolf_leaving", 0)
    autosave = true
end

function on_create()
    for _, name in ipairs({ "FloorText", "ScoreText", "ItemsText", "SecretsText", "MapText", "GoldKeyIcon",
                            "BlueKeyIcon", "ToastText" }) do
        ids[name] = ui.find(name)
    end
    if gamestate.get("wolf_floor_started", 0) ~= level then
        start_floor()
    end
    set_text("FloorText", title)
    if ids.ToastText ~= 0 then
        ui.set_visible(ids.ToastText, false)
    end
    ui.transition_fade_in(0.4)
end

local function refresh_hud()
    local items, items_total = gamestate.get("wolf_items", 0), gamestate.get("wolf_items_total", 0)
    local secrets, secrets_total = gamestate.get("wolf_secrets", 0), gamestate.get("wolf_secrets_total", 0)
    gamestate.set("wolf_items_pct", ratio(items, items_total))
    gamestate.set("wolf_secrets_pct", ratio(secrets, secrets_total))
    set_text("ScoreText", "Score " .. gamestate.get("wolf_score", 0))
    set_text("ItemsText", string.format("Items %d / %d  (%d%%)", items, items_total, ratio(items, items_total)))
    set_text("SecretsText", string.format("Secrets %d / %d  (%d%%)", secrets, secrets_total,
            ratio(secrets, secrets_total)))
    set_text("MapText", string.format("Map explored %d%%", gamestate.get("wolf_explored", 0)))
    for name, key in pairs({ GoldKeyIcon = "wolf_key_gold", BlueKeyIcon = "wolf_key_blue" }) do
        local has = gamestate.get(key, 0) == 1
        if ids[name] ~= 0 and shown[name] ~= has then
            shown[name] = has
            ui.set_visible(ids[name], has)
        end
    end
end

local function toast(text)
    set_text("ToastText", text)
    if ids.ToastText ~= 0 then
        ui.set_visible(ids.ToastText, true)
    end
    toast_left = TOAST_SECONDS
end

function on_update(dt)
    refresh_hud()
    local message = gamestate.get("wolf_toast", "")
    if message ~= "" then
        gamestate.set("wolf_toast", "")
        toast(message)
    end
    if toast_left > 0 then
        toast_left = toast_left - dt
        if toast_left <= 0 and ids.ToastText ~= 0 then
            ui.set_visible(ids.ToastText, false)
        end
    end
    if ui.is_transition_active() or leaving then
        return
    end
    -- Saved once the floor runs: the save holds the scene as it starts.
    if autosave then
        autosave = false
        save.save_game(AUTOSAVE_SLOT)
    end
    if input.is_action_just_pressed("quick_save") then
        save.save_game(QUICK_SLOT)
        toast("Game saved (F9 loads it)")
    elseif input.is_action_just_pressed("quick_load") and save.has_save(QUICK_SLOT) then
        save.load_game(QUICK_SLOT)
    elseif input.is_action_just_pressed("pause") then
        leaving = true
        scene.transition_to("scenes/title.owl", "fade", 0.3)
    end
end
