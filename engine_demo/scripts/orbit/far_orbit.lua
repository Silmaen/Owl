-- Far orbit scene script, opened by the telescope of the observatory of the hub: a station, a moon and a relay one
-- astronomical unit from the world's zero, where a float steps by 16 km. The scene's floating origin keeps the camera
-- near zero: every root entity carries a `WorldPosition` in double, so the ruler beads by the debris, 20 cm apart, stay
-- distinct. The camera is the cockpit of a ship (`ShipController`: mouse to steer, A / D to roll, W / S to thrust, Z
-- for the flight assist), its dashboard on the HUD. The objective: dock with the debris drifting past the station (fly
-- within `dock_distance` of it, its `HudMarker` shows the way), which marks the place done and goes back to the hub. J
-- jumps between the station and the relay, 400 000 km apart; Escape (or Start) goes back to the hub without completing
-- it. The planet Cinder hangs below the station (a planet layer, drawn in scaled space from the relay), and debris
-- fields stream in as world partition cells along the way. The moon pulls three moonlets (a `GravitySource`, the
-- scene's world gravity is zero): launched on circular orbits at the first update, they follow them in the solver.
-- Period and comma raise and lower the time warp (1, 10, 100, 1000): above the scene's rails time scale the moonlets go
-- on rails, their conic arcs computed in closed form. At the relay their `SimulationLod` demotes them: they keep
-- orbiting on a coarse tick and are back in the solver on the return.
-- Demonstrates: transform.get_world_position / set_world_position (double precision), scene.get_world_origin, the
-- floating origin of a scene (`FloatingOrigin:`), the world partition (`WorldPartition:`), a `Planet`,
-- scene.get_loaded_world_cells, scene.get_planet_altitude, a distance in double, ui.set_text, entity.get_ship_speed,
-- entity.get_ship_throttle, entity.is_ship_assist_on, ui.set_progress, ui.set_visible, physics.get_gravity,
-- physics.set_velocity, physics.is_on_rails, time.set_scale, entity.is_demoted.

properties = {
    { name = "hub_scene", type = "string", default = "scenes/world_map.owl" },
    { name = "camera", type = "string", default = "OrbitCamera" },
    { name = "relay_offset", type = "float", default = 400000000.0 },  -- metres from the station to the relay, along x
    { name = "planet", type = "string", default = "Cinder" },
    { name = "debris", type = "string", default = "Debris" },
    { name = "dock_distance", type = "float", default = 6.0, min = 0.5 },
    { name = "docked_delay", type = "float", default = 1.5, min = 0 },
    { name = "moon", type = "string", default = "Moon" },
    { name = "moonlet_count", type = "int", default = 3, min = 0 },
}

local warp_levels = { 1, 10, 100, 1000 }
local warp_index = 1
local warp_id = 0
local moon_id = 0
local moonlets = {}
local moonlet_radii = {}
local launched = false

local readout_id = 0
local objective_id = 0
local speed_id = 0
local dock_id = 0
local throttle_id = 0
local assist_on_id = 0
local assist_off_id = 0
local assist_shown = nil
local camera_id = 0
local planet_id = 0
local debris_id = 0
local leaving = false
local at_relay = false
local docked_clock = -1

function on_create()
    readout_id = ui.find("WorldReadout")
    objective_id = ui.find("ObjectiveText")
    speed_id = ui.find("SpeedText")
    dock_id = ui.find("DockText")
    throttle_id = ui.find("ThrottleBar")
    assist_on_id = ui.find("AssistOn")
    assist_off_id = ui.find("AssistOff")
    camera_id = scene.find_entity(camera)
    planet_id = scene.find_entity(planet)
    debris_id = scene.find_entity(debris)
    warp_id = ui.find("WarpText")
    moon_id = scene.find_entity(moon)
    for index = 0, moonlet_count - 1 do
        local id = scene.find_entity("Moonlet" .. index)
        if id ~= 0 then
            moonlets[#moonlets + 1] = id
        end
    end
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

-- The dashboard: speed, throttle, flight assist and the distance to the debris.
local function update_dashboard(distance)
    ui.set_text(speed_id, string.format("%.0f m/s", entity.get_ship_speed(camera_id) or 0))
    ui.set_progress(throttle_id, math.abs(entity.get_ship_throttle(camera_id) or 0))
    local assist = entity.is_ship_assist_on(camera_id) ~= false
    if assist ~= assist_shown then
        assist_shown = assist
        ui.set_visible(assist_on_id, assist)
        ui.set_visible(assist_off_id, not assist)
    end
    if distance < 10000 then
        ui.set_text(dock_id, string.format("DOCK %.0f m", distance))
    else
        ui.set_text(dock_id, string.format("DOCK %.0f km", distance / 1000))
    end
end

-- Offset of a moonlet from the moon, in metres.
local function moon_offset(id)
    local mx, my, mz = transform.get_world_position(moon_id)
    local x, y, z = transform.get_world_position(id)
    return x - mx, y - my, z - mz
end

-- Each moonlet on a circular orbit around the moon's Y axis, at the speed its pull asks: v = sqrt(g r).
local function launch_moonlets()
    launched = true
    if moon_id == 0 then
        return
    end
    for index, id in ipairs(moonlets) do
        local rx, ry, rz = moon_offset(id)
        local r = math.sqrt(rx * rx + ry * ry + rz * rz)
        -- The physics works around the floating origin: the local position of the root entity.
        local gx, gy, gz = physics.get_gravity(transform.get_position(id))
        local speed = math.sqrt(math.sqrt(gx * gx + gy * gy + gz * gz) * r)
        local flat = math.sqrt(rx * rx + rz * rz)
        physics.set_velocity(id, speed * rz / flat, 0, -speed * rx / flat)
        moonlet_radii[index] = r
    end
    gamestate.set("orbit_launched", 1)
end

-- The time warp and the moonlets: warp level, rails, demoted count and how far the orbits drifted.
local function update_warp()
    if input.is_action_just_pressed("orbit_warp_up") and warp_index < #warp_levels then
        warp_index = warp_index + 1
        time.set_scale(warp_levels[warp_index])
    elseif input.is_action_just_pressed("orbit_warp_down") and warp_index > 1 then
        warp_index = warp_index - 1
        time.set_scale(warp_levels[warp_index])
    end
    local demoted = 0
    local drift = 0
    for index, id in ipairs(moonlets) do
        if entity.is_demoted(id) then
            demoted = demoted + 1
        end
        local rx, ry, rz = moon_offset(id)
        drift = math.max(drift, math.abs(math.sqrt(rx * rx + ry * ry + rz * rz) - moonlet_radii[index]) / moonlet_radii[index])
    end
    local rails = physics.is_on_rails()
    gamestate.set("orbit_warp", warp_levels[warp_index])
    gamestate.set("orbit_rails", rails and 1 or 0)
    gamestate.set("orbit_demoted", demoted)
    gamestate.set("orbit_drift", drift)
    ui.set_text(warp_id, string.format("WARP x%d%s", warp_levels[warp_index], rails and "  ON RAILS" or ""))
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
    if not launched then
        launch_moonlets()
    end
    update_warp()
    local x, y, z = transform.get_world_position(camera_id)
    if input.is_action_just_pressed("orbit_jump") then
        local step = at_relay and -relay_offset or relay_offset
        at_relay = not at_relay
        x = x + step
        transform.set_world_position(camera_id, x, y, z)
    end
    local distance = debris_distance(x, y, z)
    update_dashboard(distance)
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
