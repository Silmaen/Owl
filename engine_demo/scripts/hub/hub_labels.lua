-- Name tags of the hub: each villager carries a label that follows it and grows in when the hero is near.
-- Demonstrates: world `TextRenderer` labels driven from Lua (`transform.set_scale`: the text height is the Y scale, so a
--               label grows from nothing to its size, and `ui.set_visible` hides it once it is gone), an entity that
--               follows another (`transform.get_position` / `set_position`), a distance with a hysteresis (the tag
--               appears at NEAR cells and goes at FAR, so it does not flicker at the limit).
--
-- The entities `Tag_<name>` hold the name of the character `<name>` and float above it. The only other texts of the
-- hub are the five level titles of the houses, plain labels of the scene that are always shown. `labels_shown` in
-- gamestate counts the tags that are fully grown.

properties = {
    { name = "tag_near", type = "float", default = 4.0, min = 0 },
    { name = "tag_far", type = "float", default = 5.0, min = 0 },  -- cells from the hero at which a tag appears and disappears
    { name = "grow_speed", type = "float", default = 6.0, min = 0 },  -- per second: a tag takes a sixth of a second to grow
    { name = "tag_height", type = "float", default = 0.95 },  -- cells above the character
}

local TAGS = {
    "Villager", "Guard", "Wizard", "Knight", "Farmer", "Merchant1", "Merchant2", "Merchant3", "Priest", "Woodcutter",
}
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
    for _, name in ipairs(TAGS) do
        add(scene.find_entity("Tag_" .. name), scene.find_entity(name), tag_near, tag_far, true)
    end
end

function on_update(dt)
    local px, py = transform.get_position(player_id)
    local full = 0
    for _, item in ipairs(items) do
        local x, y = item.x, item.y
        if item.follows then
            x, y = transform.get_position(item.target)
            y = y + tag_height
        end
        local dist = math.sqrt((px - x) ^ 2 + (py - y) ^ 2)
        local want = dist < (item.grown > 0 and item.far or item.near) and 1 or 0
        local grown = item.grown
        if grown ~= want then
            grown = want > grown and math.min(want, grown + dt * grow_speed) or math.max(want, grown - dt * grow_speed)
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
