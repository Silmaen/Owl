-- Labels of the hub that grow in when the hero is near, and the name tags that follow the villagers.
-- Demonstrates: world `TextRenderer` labels driven from Lua (`transform.set_scale`: the text height is the Y scale, so a
--               label grows from nothing to its size, and `ui.set_visible` hides it once it is gone), an entity that
--               follows another (`transform.get_position` / `set_position`), a distance with a hysteresis (the label
--               appears at NEAR cells and goes at FAR, so it does not flicker at the limit).
--
-- The place names (church, square, houses, shrine...) are plain labels of the scene that are always shown. The entities
-- `Hint_<name>` (fields, stalls, signposts, river...) only appear near the hero. The entities `Tag_<name>` hold the name of
-- the character `<name>` and float above it. `labels_shown` in gamestate counts the labels that are fully grown.

local HINTS = {
    "Sign1", "Sign2", "Sign3", "Sign4", "Danger", "Well", "Bakery", "Dairy", "Cider", "Wheat", "Cabbages", "Carrots",
    "Flowers", "Woodcutter", "RiverW", "RiverE", "RiverC", "Bridge", "Lever", "Gate", "Market",
}
local TAGS = {
    "Villager", "Guard", "Wizard", "Knight", "Farmer", "Merchant1", "Merchant2", "Merchant3", "Priest", "Woodcutter",
}
local NEAR, FAR = 6.5, 8.0      -- cells from the hero at which a label appears and disappears
local TAG_NEAR, TAG_FAR = 4.0, 5.0
local GROW_SPEED = 6.0          -- per second: a label takes a sixth of a second to grow
local TAG_HEIGHT = 0.95         -- cells above the character

local player_id = 0
local items = {}
local shown = -1

local function add(id, target, near, far, follows)
    if id == 0 then
        return
    end
    local x, y, z = transform.get_position(id)
    local _, em = transform.get_scale(id)
    items[#items + 1] = {
        id = id, target = target, near = near, far = far, follows = follows,
        x = x, y = y, z = z, em = em, grown = 0, hidden = false,
    }
    ui.set_visible(id, false)
    transform.set_scale(id, 1, 0.001, 1)
end

function on_create()
    player_id = scene.find_entity("Player")
    for _, name in ipairs(HINTS) do
        local id = scene.find_entity("Hint_" .. name)
        add(id, id, NEAR, FAR, false)
    end
    for _, name in ipairs(TAGS) do
        add(scene.find_entity("Tag_" .. name), scene.find_entity(name), TAG_NEAR, TAG_FAR, true)
    end
end

function on_update(dt)
    local px, py = transform.get_position(player_id)
    local full = 0
    for _, item in ipairs(items) do
        local x, y = item.x, item.y
        if item.follows then
            x, y = transform.get_position(item.target)
            y = y + TAG_HEIGHT
        end
        local dist = math.sqrt((px - x) ^ 2 + (py - y) ^ 2)
        local want = dist < (item.grown > 0 and item.far or item.near) and 1 or 0
        local grown = item.grown
        if grown ~= want then
            grown = want > grown and math.min(want, grown + dt * GROW_SPEED) or math.max(want, grown - dt * GROW_SPEED)
            item.grown = grown
            ui.set_visible(item.id, grown > 0)
            transform.set_scale(item.id, 1, math.max(item.em * grown, 0.001), 1)
        end
        if grown > 0 and item.follows then
            transform.set_position(item.id, x, y, item.z)
        end
        full = full + (grown >= 1 and 1 or 0)
    end
    if full ~= shown then
        shown = full
        gamestate.set("labels_shown", full)
    end
end
