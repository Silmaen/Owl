-- A pick-up of Wolfenstein mini: treasure, food or a key, taken by walking onto it.
-- Bound by Trigger.Type=LuaCallback with CallbackName=on_pickup (fires every frame while the player overlaps).
-- Every pick-up counts for the "items found" ratio of the tally (`wolf_items`); a treasure adds to the score, a key
-- opens the doors of its colour until the end of the floor (`wolf_key_gold`, `wolf_key_blue`).
-- `item` is the kind, also the tag of the entity (level.lua counts the pick-ups of a floor by these tags).
-- Taking one bursts the sparks of the `GoldSparkle` entity (`BlueSparkle` for the blue key) just ahead of the view and
-- low, where the pick-up lay; `wolf_sparkles` counts the bursts.

properties = {
    { name = "item", type = "string", default = "Cross" },
    { name = "sparks", type = "int", default = 28, min = 0 },
    { name = "sparkle_ahead", type = "float", default = 0.55, min = 0 },  -- cells ahead of the player
    { name = "sparkle_height", type = "float", default = -0.2 },          -- above the eye: the floor is at -0.5
}

local KINDS = {
    Cross = { score = 100, label = "Cross", sound = "sounds/game/treasure.wav" },
    Chalice = { score = 500, label = "Chalice", sound = "sounds/game/treasure.wav" },
    Chest = { score = 1000, label = "Treasure chest", sound = "sounds/game/treasure.wav" },
    Crown = { score = 5000, label = "Crown", sound = "sounds/game/treasure.wav" },
    Food = { score = 20, label = "Roast dinner", sound = "sounds/game/food.wav" },
    DogFood = { score = 10, label = "Dog food", sound = "sounds/game/food.wav" },
    GoldKey = { key = "gold", label = "Gold key", sound = "sounds/game/key.wav" },
    BlueKey = { key = "blue", label = "Blue key", sound = "sounds/game/key.wav", sparkle = "BlueSparkle" },
}

local collected = false

-- A burst of sparks ahead of the player's view, where the pick-up was walked over.
local function sparkle(name, other_id)
    local id = scene.find_entity(name)
    if id == 0 or sparks <= 0 then
        return
    end
    local x, y = transform.get_position(other_id)
    local _, _, yaw = transform.get_rotation(other_id)
    transform.set_position(id, x - math.sin(yaw) * sparkle_ahead, y + math.cos(yaw) * sparkle_ahead, sparkle_height)
    if entity.particles_emit(id, sparks) then
        gamestate.set("wolf_sparkles", gamestate.get("wolf_sparkles", 0) + 1)
    end
end

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
    sparkle(kind.sparkle or "GoldSparkle", other_id)
    scene.destroy_entity(entity_id)
end
