-- The upgrades screen of Tower defence, reached from the world map: the stars won on the levels are spent on the
-- upgrades of six families (the three towers, the two spells, the treasury), three tiers each, bought in order.
--
-- - The stars to spend are the best stars of every level (`tower_stars_NN` settings) less the price of the tiers
--   bought: replaying a level for more stars credits the difference.
-- - A tier costs its number of stars (1, 2, then 3); the tier of a family is the `tower_up_<family>` setting, which
--   defence.lua applies to every level. The spells can be raised once a level has unlocked them.
-- - Refund all gives every star back for free; World map (Escape, pad B) returns to the map.
-- - Mirrored in the game state for the scenarios: `tower_up_stars` (to spend), `tower_up_won`, `tower_up_<family>`.

properties = {
    { name = "map_scene", type = "string", default = "scenes/world_map.owl" },
    { name = "levels", type = "int", default = 5, min = 1 },
    { name = "wipe_color", type = "color", default = { 0.1, 0.14, 0.24, 1 } },
}

local FAMILIES = {
    { key = "archer", name = "Archers", tiers = { "Bodkin arrows: +15% damage", "Longbows: +12% range",
                                                  "Drilled archers: shoot 15% faster" } },
    { key = "cannon", name = "Cannons", tiers = { "Guild foundry: cannons cost 15% less",
                                                  "Black powder: 20% wider bursts", "Heavy shells: +20% damage" } },
    { key = "frost", name = "Frost", tiers = { "Deep cold: the chill lasts 30% longer",
                                               "Bitter frost: chilled enemies are slower", "Rime: +12% range" } },
    { key = "rain", name = "Rain of arrows", unlock = "tower_done_02", unlock_text = "win Thornwood Pass",
      tiers = { "Fletchers: ready 20% sooner", "Broadheads: +30% damage", "Volley: a 25% wider rain" } },
    { key = "shield", name = "Shield wall", unlock = "tower_done_04", unlock_text = "win Frostmere",
      tiers = { "Drill: ready 20% sooner", "Iron wall: holds 2 seconds more", "Wide wall: 25% wider" } },
    { key = "treasury", name = "Treasury", tiers = { "War chest: +40 gold at the start",
                                                     "Bounties: +10% gold per kill", "Masons: +2 lives" } },
}
local TIERS = 3

local ids = {}
local leaving = false

local function setting_key(family)
    return "tower_up_" .. family.key
end

local function tier_of(family)
    return settings.get(setting_key(family), 0)
end

local function stars_won()
    local total = 0
    for number = 1, levels do total = total + settings.get(string.format("tower_stars_%02d", number), 0) end
    return total
end

local function stars_spent()
    local total = 0
    for _, family in ipairs(FAMILIES) do
        local bought = tier_of(family)
        total = total + bought * (bought + 1) // 2
    end
    return total
end

local function unlocked(family)
    return family.unlock == nil or settings.get(family.unlock, false) == true
end

local function refresh()
    local won, spent = stars_won(), stars_spent()
    local free = won - spent
    ui.set_text(ids.stars, string.format("Stars to spend: %d   (won %d of %d)", free, won, levels * 3))
    for index, family in ipairs(FAMILIES) do
        local row = ids.rows[index]
        local bought = tier_of(family)
        local cost = bought + 1
        ui.set_text(row.name, family.name)
        local text
        if not unlocked(family) then
            text = "Locked: " .. family.unlock_text
        elseif bought >= TIERS then
            text = "All bought: " .. family.tiers[TIERS]
        else
            text = family.tiers[cost]
        end
        ui.set_text(row.text, text)
        for pip, id in ipairs(row.pips) do ui.set_image_frame(id, pip <= bought and 1 or 0) end
        ui.set_text(row.cost, bought >= TIERS and "" or (cost .. (cost == 1 and " star" or " stars")))
        local can = unlocked(family) and bought < TIERS and free >= cost
        ui.set_button_enabled(row.buy, can)
        ui.set_text(row.buy, not unlocked(family) and "Locked" or (bought >= TIERS and "Bought" or "Buy"))
        ui.set_visible(row.plank, can)
        gamestate.set(setting_key(family), bought)
    end
    gamestate.set("tower_up_stars", free)
    gamestate.set("tower_up_won", won)
end

local function buy(index)
    local family = FAMILIES[index]
    if family == nil or not unlocked(family) then return end
    local bought = tier_of(family)
    if bought >= TIERS or stars_won() - stars_spent() < bought + 1 then return end
    settings.set(setting_key(family), bought + 1)
    settings.save()
    sound.play("sounds/game/build.wav")
    refresh()
end

function on_buy_1() buy(1) end
function on_buy_2() buy(2) end
function on_buy_3() buy(3) end
function on_buy_4() buy(4) end
function on_buy_5() buy(5) end
function on_buy_6() buy(6) end

function on_refund_clicked()
    for _, family in ipairs(FAMILIES) do settings.set(setting_key(family), 0) end
    settings.save()
    sound.play("sounds/game/coins.wav")
    refresh()
end

function on_back_clicked()
    if leaving then return end
    leaving = true
    scene.transition_to(map_scene, "wipe_down", 0.35, wipe_color.r, wipe_color.g, wipe_color.b)
end

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("wipe_down", 0.4, wipe_color.r, wipe_color.g, wipe_color.b)
    end
    ids.stars = scene.find_entity("StarsText")
    ids.rows = {}
    for index = 1, #FAMILIES do
        local pips = scene.find_entities("UpgradePip" .. index)
        ids.rows[index] = { name = scene.find_entity("UpgradeName" .. index),
                            text = scene.find_entity("UpgradeText" .. index),
                            cost = scene.find_entity("UpgradeCost" .. index),
                            buy = scene.find_entity("UpgradeBuy" .. index),
                            plank = scene.find_entity("UpgradeBuy" .. index .. "Frame"), pips = pips }
    end
    gamestate.set("tower_upgrades_screen", true)
    refresh()
end

function on_update(dt)
    if leaving then return end
    if input.is_action_just_pressed("ui_cancel") or input.is_action_just_pressed("cancel") then on_back_clicked() end
end
