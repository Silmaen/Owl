-- Minimap of the hub: the tilemap of the village drawn a second time, small, in the `overlay` layer of the scene (a
-- `Space: Screen` Renderer2D layer, 720 units high), with a dot for the hero and one per place left (the doors hide
-- theirs once the place is done, place_door.lua). This script keeps the hero's dot on the map.
-- Demonstrates: one tilemap asset drawn by two entities on two layers of the renderer stack, a screen-space layer
-- composed over the 2D world, transform.set_position on a screen-space entity.

properties = {
    { name = "player", type = "string", default = "Player" },
    { name = "dot", type = "string", default = "MinimapPlayer" },
    { name = "origin_x", type = "float", default = 16 },  -- screen position of the north west corner of the map...
    { name = "origin_y", type = "float", default = 16 },  -- ...measured from the bottom left of the screen
    { name = "scale", type = "float", default = 3 },      -- screen units per cell
    { name = "rows", type = "int", default = 40 },
}

local player_id = 0
local dot_id = 0

function on_create()
    player_id = scene.find_entity(player)
    dot_id = scene.find_entity(dot)
end

function on_update(_)
    if player_id == 0 or dot_id == 0 then
        return
    end
    local px, py = transform.get_position(player_id)
    local _, _, z = transform.get_position(dot_id)
    -- The cell (c, r) of the map has its centre at (c + 0.5, -r - 0.5): y counts up from the south edge on the screen.
    local x = origin_x + px * scale
    local y = origin_y + (rows + py) * scale
    transform.set_position(dot_id, x, y, z)
    gamestate.set("minimap_x", math.floor(x))
    gamestate.set("minimap_y", math.floor(y))
end
