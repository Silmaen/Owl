-- Far orbit scene script: a station, a moon and a relay one astronomical unit from the world's zero, where a float
-- steps by 16 km. The scene's floating origin keeps the camera near zero: every root entity carries a `WorldPosition`
-- in double, so the gauge beads, one centimetre apart, stay distinct. J jumps between the station and the relay,
-- 400 000 km apart; Escape (or Start) goes back to the menu.
-- Demonstrates: transform.get_world_position / set_world_position (double precision), scene.get_world_origin,
-- the floating origin of a scene (`FloatingOrigin:`), ui.set_text.

properties = {
    { name = "menu_scene", type = "string", default = "scenes/main_menu.owl" },
    { name = "camera", type = "string", default = "OrbitCamera" },
    { name = "relay_offset", type = "float", default = 400000000.0 },  -- metres from the station to the relay, along x
}

local readout_id = 0
local camera_id = 0
local leaving = false
local at_relay = false

function on_create()
    readout_id = ui.find("WorldReadout")
    camera_id = scene.find_entity(camera)
    ui.transition_fade_in(0.4)
end

function on_update(_)
    if leaving or ui.is_transition_active() then
        return
    end
    if input.is_action_just_pressed("pause") then
        leaving = true
        scene.transition_to(menu_scene, "fade_out", 0.3)
        return
    end
    if camera_id == 0 then
        return
    end
    local x, y, z = transform.get_world_position(camera_id)
    if input.is_action_just_pressed("orbit_jump") then
        local step = at_relay and -relay_offset or relay_offset
        at_relay = not at_relay
        x = x + step
        transform.set_world_position(camera_id, x, y, z)
    end
    local ox, oy, oz = scene.get_world_origin()
    ui.set_text(readout_id, string.format("Camera in the world: %.3f, %.3f, %.3f m\nWorld origin: %.0f, %.0f, %.0f m",
        x, y, z, ox, oy, oz))
end
