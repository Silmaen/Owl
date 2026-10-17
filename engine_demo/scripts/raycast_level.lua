-- Logic of the raycast level (E1M1): the secrets and the HUD counters.
-- Demonstrates: the `pushwall` table (`pushwall.get_state` polled on every secret wall), `scene.find_entity`, a HUD
--               text refreshed when its value changes, the inventory counters of gamestate (`inv_<item>`).
--
-- The five secret walls slide when the player presses E near them (built-in activation of `RaycastPushWall`); a wall
-- that left the `idle` state counts as a found secret (`raycast_secrets` in gamestate, kept for the whole run).

local SECRETS = { "Secret1", "Secret2", "Secret3", "Secret4", "Secret5" }
local TREASURES = { "cross", "chalice", "chest", "crown" }
local TREASURE_TOTAL = 11

local walls = {}
local found = {}
local treasure_id = 0
local shown = ""

function on_create()
    treasure_id = ui.find("TreasureText")
    for i, name in ipairs(SECRETS) do
        walls[i] = scene.find_entity(name)
        found[i] = gamestate.get("secret_found_" .. name, 0) > 0
    end
end

function on_update(dt)
    local secrets = 0
    for i, name in ipairs(SECRETS) do
        if not found[i] and walls[i] ~= 0 and pushwall.get_state(walls[i]) ~= "idle" then
            found[i] = true
            gamestate.set("secret_found_" .. name, 1)
            gamestate.set("inv_toast", "Secret found!")
        end
        secrets = secrets + (found[i] and 1 or 0)
    end
    gamestate.set("raycast_secrets", secrets)

    local treasure = 0
    for _, id in ipairs(TREASURES) do
        treasure = treasure + gamestate.get("inv_" .. id, 0)
    end
    local text = "Treasure: " .. treasure .. " / " .. TREASURE_TOTAL .. "    Secrets: " .. secrets .. " / " .. #SECRETS
    if text ~= shown then
        shown = text
        ui.set_text(treasure_id, text)
    end
end
