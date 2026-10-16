-- Cloud of the isometric town: a sprite of the 2D `world` layer, drawn behind the isometric layer.
-- Demonstrates: two rendering styles in one scene (the 2D layer shows the sky and the clouds around the isometric
--               map, the isometric layer draws the town over it), a parallax following the scene camera.
--
-- `parallax` 1 pins the cloud to the world, 0 pins it to the screen; `speed` drifts it, in world units per second.

properties = {
    { name = "offset_x", type = "float", default = 0.0 },
    { name = "offset_y", type = "float", default = 0.0 },
    { name = "parallax", type = "float", default = 0.4 },
    { name = "speed", type = "float", default = 0.3 },
}

local DRIFT_RANGE = 20
local camera_id = 0
local drift = 0

function on_create()
    camera_id = scene.find_entity("MainCamera")
end

function on_update(dt)
    drift = drift + speed * dt
    if drift > DRIFT_RANGE then
        drift = -DRIFT_RANGE
    end
    local camera_x, camera_y = transform.get_position(camera_id)
    local _, _, z = transform.get_position(entity_id)
    transform.set_position(entity_id, camera_x * (1 - parallax) + offset_x + drift,
                           camera_y * (1 - parallax) + offset_y, z)
end
