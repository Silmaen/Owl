-- A pick-up of Wolfenstein mini: treasure, food or a key, taken by walking onto it.
-- Bound by Trigger.Type=LuaCallback with CallbackName=on_pickup (fires every frame while the player overlaps).
-- Every pick-up counts for the "items found" ratio of the tally (`wolf_items`); a treasure adds to the score, a key
-- opens the doors of its colour until the end of the floor (`wolf_key_gold`, `wolf_key_blue`).
-- `item` is the kind, also the tag of the entity (level.lua counts the pick-ups of a floor by these tags).

properties = {
    { name = "item", type = "string", default = "Cross" },
}

local KINDS = {
    Cross = { score = 100, label = "Cross", sound = "sounds/game/treasure.wav" },
    Chalice = { score = 500, label = "Chalice", sound = "sounds/game/treasure.wav" },
    Chest = { score = 1000, label = "Treasure chest", sound = "sounds/game/treasure.wav" },
    Crown = { score = 5000, label = "Crown", sound = "sounds/game/treasure.wav" },
    Food = { score = 20, label = "Roast dinner", sound = "sounds/game/food.wav" },
    DogFood = { score = 10, label = "Dog food", sound = "sounds/game/food.wav" },
    GoldKey = { key = "gold", label = "Gold key", sound = "sounds/game/key.wav" },
    BlueKey = { key = "blue", label = "Blue key", sound = "sounds/game/key.wav" },
}

local collected = false

function on_pickup(other_id)
    local kind = KINDS[item]
    if collected or kind == nil then
        return
    end
    collected = true
    gamestate.set("wolf_items", gamestate.get("wolf_items", 0) + 1)
    if kind.score then
        gamestate.set("wolf_score", gamestate.get("wolf_score", 0) + kind.score)
        gamestate.set("wolf_toast", kind.label .. "  +" .. kind.score)
    end
    if kind.key then
        gamestate.set("wolf_key_" .. kind.key, 1)
        gamestate.set("wolf_toast", kind.label .. ": it opens the " .. kind.key .. " doors")
    end
    sound.play(kind.sound)
    scene.destroy_entity(entity_id)
end
