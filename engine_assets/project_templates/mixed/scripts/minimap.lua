-- Minimap dot: follows the 3D player on the 2D minimap layer (screen units, origin bottom left, y up).
-- The world x axis runs to the right of the map, the world z axis down it.

properties = {
    {name = "target", type = "string", default = "Player"},
    {name = "centre_x", type = "float", default = 130.0},
    {name = "centre_y", type = "float", default = 130.0},
    {name = "pixels_per_metre", type = "float", default = 6.0},
}

local player = 0

function on_create()
    player = scene.find_entity(target)
end

function on_update(dt)
    if player == 0 then
        return
    end
    local x, _, z = transform.get_position(player)
    local _, _, depth = transform.get_position(entity_id)
    transform.set_position(entity_id, centre_x + x * pixels_per_metre, centre_y - z * pixels_per_metre, depth)
end
