-- Collectible item: walking over it adds it to the inventory.
-- Demonstrates: Trigger.Type=LuaCallback, string properties, gamestate as the inventory (kept in saves),
--               entity destruction, sound.play (one sound per kind of item). Inventory window: inventory.lua.
--
-- The inventory is the gamestate keys `inv_<item>` (counts); `picked_<tag>` remembers a taken pickup so that a
-- scene loaded again, or entered again, does not bring it back. `inv_toast` is the message `inventory.lua` shows.

-- Each kind of item has its own sound (synthesised by sounds/generate_sounds.py): a jingle for the treasures, a bite for
-- food and health, a click-clack for ammunition and weapons.
local SOUNDS = {
    cross = "sounds/raycast_treasure.wav", chalice = "sounds/raycast_treasure.wav",
    chest = "sounds/raycast_treasure.wav", crown = "sounds/raycast_treasure.wav",
    food = "sounds/raycast_food.wav", health = "sounds/raycast_food.wav",
    ammo = "sounds/raycast_ammo.wav", gun = "sounds/raycast_ammo.wav",
}

properties = {
    { name = "item", type = "string", default = "" },
    { name = "label", type = "string", default = "" },
}

local collected = false

function on_create()
    if gamestate.get("picked_" .. entity.get_name(entity_id), false) then
        collected = true
        scene.destroy_entity(entity_id)
    end
end

-- Bound by Trigger.Type=LuaCallback with CallbackName=on_pickup (fires every frame while the player overlaps).
function on_pickup(other_id)
    if collected or item == "" then
        return
    end
    collected = true
    local key = "inv_" .. item
    gamestate.set(key, gamestate.get(key, 0) + 1)
    gamestate.set("picked_" .. entity.get_name(entity_id), true)
    gamestate.set("inv_toast", "Picked up: " .. label)
    sound.play(SOUNDS[item] or "sounds/coin_collect.wav")
    scene.destroy_entity(entity_id)
end
