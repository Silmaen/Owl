-- Far orbit scene script, opened by the telescope of the observatory of the hub: a station, a moon and a relay one
-- astronomical unit from the world's zero, where a float steps by 16 km. The scene's floating origin keeps the camera
-- near zero: every root entity carries a `WorldPosition` in double, so the gauge beads, one centimetre apart, stay
-- distinct. The objective: dock with the debris drifting off the station (fly within `dock_distance` of it, its
-- `HudMarker` shows the way), which marks the place done and goes back to the hub. J jumps between the station and the
-- relay, 400 000 km apart; Escape (or Start) goes back to the hub without completing it. The planet Cinder hangs below
-- the station (a planet layer, drawn in scaled space from the relay), and debris fields stream in as world partition
-- cells along the way.
-- Demonstrates: transform.get_world_position / set_world_position (double precision), scene.get_world_origin,
-- the floating origin of a scene (`FloatingOrigin:`), the world partition (`WorldPartition:`), a `Planet`,
-- scene.get_loaded_world_cells, scene.get_planet_altitude, a distance in double, ui.set_text.

properties = {
    { name = "hub_scene", type = "string", default = "scenes/world_map.owl" },
    { name = "camera", type = "string", default = "OrbitCamera" },
    { name = "relay_offset", type = "float", default = 400000000.0 },  -- metres from the station to the relay, along x
    { name = "planet", type = "string", default = "Cinder" },
    { name = "debris", type = "string", default = "Debris" },
    { name = "dock_distance", type = "float", default = 6.0, min = 0.5 },
    { name = "docked_delay", type = "float", default = 1.5, min = 0 },
}

local readout_id = 0
local objective_id = 0
local camera_id = 0
local planet_id = 0
local debris_id = 0
local leaving = false
local at_relay = false
local docked_clock = -1

function on_create()
    readout_id = ui.find("WorldReadout")
    objective_id = ui.find("ObjectiveText")
    camera_id = scene.find_entity(camera)
    planet_id = scene.find_entity(planet)
    debris_id = scene.find_entity(debris)
    gamestate.set("orbit_docked", 0)
    ui.transition_fade_in(0.4)
end

local function leave()
    leaving = true
    scene.transition_to(hub_scene, "fade_out", 0.4)
end

-- Distance from the camera to the debris, in metres, from the double world positions.
local function debris_distance(x, y, z)
    if debris_id == 0 then
        return math.huge
    end
    local dx, dy, dz = transform.get_world_position(debris_id)
    return math.sqrt((dx - x) ^ 2 + (dy - y) ^ 2 + (dz - z) ^ 2)
end

function on_update(dt)
    if leaving or ui.is_transition_active() then
        return
    end
    if docked_clock >= 0 then
        docked_clock = docked_clock + dt
        if docked_clock >= docked_delay then
            leave()
        end
        return
    end
    if input.is_action_just_pressed("pause") then
        leave()
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
    local distance = debris_distance(x, y, z)
    if distance <= dock_distance then
        docked_clock = 0
        gamestate.set("orbit_docked", 1)
        gamestate.set("level_orbit_done", 1)
        entity.set_hud_marker_enabled(debris_id, false)
        sound.play("sounds/ui/victory.wav")
        ui.set_text(objective_id, "Docked with the debris: back to the observatory")
        return
    end
    ui.set_text(objective_id, string.format("Objective: dock with the debris (%.0f m)", distance))
    local ox, oy, oz = scene.get_world_origin()
    local altitude = planet_id ~= 0 and scene.get_planet_altitude(planet_id, camera_id) or 0
    ui.set_text(readout_id, string.format(
        "Camera in the world: %.3f, %.3f, %.3f m\nWorld origin: %.0f, %.0f, %.0f m\n%s altitude: %.1f km, cells loaded: %d",
        x, y, z, ox, oy, oz, planet, altitude / 1000.0, scene.get_loaded_world_cells()))
end
