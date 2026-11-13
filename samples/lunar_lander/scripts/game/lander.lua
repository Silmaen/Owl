-- Flight of the lander around the moon, for the graphs of Lunar Lander (`landing.owlvs`): the vector maths a graph
-- would hide. The moon is centred on the origin; the local vertical of the lander is the direction from the centre.
--
-- The lander is the `Lander` body (the hull) and four `Leg1`..`Leg4` bodies held to it by breakable fixed joints.
-- Rotations follow `math::Transform`: Euler angles (x, y, z) composed as Rz * Ry * Rx.

local M = {}

local LEGS = { "Leg1", "Leg2", "Leg3", "Leg4" }
local FEET = 1.25 -- distance from the centre of the hull down to the soles of the feet
local GROUND = 1 -- collision mask of the ground layer (0): the moon and the pads

-- Columns of the rotation matrix of Euler angles: the x, y and z axes of the body in the world.
local function axes(rx, ry, rz)
    local ca, sa = math.cos(rx), math.sin(rx)
    local cb, sb = math.cos(ry), math.sin(ry)
    local cc, sc = math.cos(rz), math.sin(rz)
    local x = { cb * cc, cb * sc, -sb }
    local y = { sa * sb * cc - ca * sc, sa * sb * sc + ca * cc, sa * cb }
    local z = { ca * sb * cc + sa * sc, ca * sb * sc - sa * cc, ca * cb }
    return x, y, z
end

local function length(x, y, z)
    return math.sqrt(x * x + y * y + z * z)
end

local function unit(x, y, z)
    local l = length(x, y, z)
    if l < 1e-9 then return 0, 1, 0 end
    return x / l, y / l, z / l
end

local function cross(ax, ay, az, bx, by, bz)
    return ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx
end

-- Euler angles (Rz * Ry * Rx) of the basis whose columns are x, y, z.
local function euler(x, y, z)
    local ry = math.asin(math.max(-1, math.min(1, -x[3])))
    if math.cos(ry) < 1e-5 then
        -- Gimbal lock: only rx - rz (or rx + rz) is known, rz is taken as 0.
        if x[3] < 0 then return math.atan(y[1], y[2]), ry, 0 end
        return math.atan(-y[1], y[2]), ry, 0
    end
    local rx = math.atan(y[3], z[3])
    local rz = math.atan(x[2], x[1])
    return rx, ry, rz
end

local function mass_of(hull)
    local total = physics.get_mass(hull)
    for _, name in ipairs(LEGS) do
        local leg = scene.find_entity(name)
        if leg ~= 0 then total = total + physics.get_mass(leg) end
    end
    return total
end

--- Fly the lander for one tick: the main engine pushes along its up axis, the stick turns it (pitch around its
--- right axis, roll around its forward axis; released, it stops turning), the RCS slides it sideways.
---@param hull entity
---@param throttle number
---@param pitch number
---@param roll number
---@param slide_x number
---@param slide_z number
---@param thrust number
---@param turn_rate number
---@param rcs number
---@return number burn
function M.fly(hull, throttle, pitch, roll, slide_x, slide_z, thrust, turn_rate, rcs)
    local rx, ry, rz = transform.get_rotation(hull)
    local right, up, back = axes(rx, ry, rz)
    local mass = mass_of(hull)
    local push = math.max(0, math.min(1, throttle)) * thrust * mass
    local fx, fy, fz = up[1] * push, up[2] * push, up[3] * push
    local side = math.abs(slide_x) + math.abs(slide_z)
    if side > 0.05 then
        local k = rcs * mass
        fx = fx + (right[1] * slide_x + back[1] * slide_z) * k
        fy = fy + (right[2] * slide_x + back[2] * slide_z) * k
        fz = fz + (right[3] * slide_x + back[3] * slide_z) * k
    end
    physics.add_force(hull, fx, fy, fz)
    -- Rate command: the angular velocity follows the stick in the frame of the lander.
    local wp, wr = pitch * turn_rate, -roll * turn_rate
    physics.set_angular_velocity(hull, right[1] * wp + back[1] * wr, right[2] * wp + back[2] * wr,
        right[3] * wp + back[3] * wr)
    return math.max(0, math.min(1, throttle)) + math.min(1, side) * 0.25
end

--- Telemetry of the lander: its altitude above the ground straight below, its vertical speed (up is positive), its
--- horizontal speed and the body under it (0 when the ray meets nothing within 400 units).
---@param hull entity
---@return number altitude
---@return number vertical
---@return number horizontal
---@return entity ground
function M.telemetry(hull)
    local x, y, z = transform.get_world_position(hull)
    local ux, uy, uz = unit(x, y, z)
    local vx, vy, vz = physics.get_velocity(hull)
    local vertical = vx * ux + vy * uy + vz * uz
    local hx, hy, hz = vx - ux * vertical, vy - uy * vertical, vz - uz * vertical
    -- From the centre of the hull, on the ground layer only (the lander's bodies are on layer 1); the feet are
    -- FEET below the centre.
    local hit = physics.raycast_3d(x, y, z, -ux, -uy, -uz, 400, GROUND)
    if hit == nil then
        return length(x, y, z) - 160 - FEET, vertical, length(hx, hy, hz), 0
    end
    return hit.distance - FEET, vertical, length(hx, hy, hz), hit.entity
end

--- Whether the lander rests: almost still, not turning, its up axis within `tilt` (cosine) of the local vertical.
---@param hull entity
---@param tilt number
---@return boolean resting
function M.resting(hull, tilt)
    local x, y, z = transform.get_world_position(hull)
    local ux, uy, uz = unit(x, y, z)
    local _, up = axes(transform.get_rotation(hull))
    local vx, vy, vz = physics.get_velocity(hull)
    local wx, wy, wz = physics.get_angular_velocity(hull)
    local upright = up[1] * ux + up[2] * uy + up[3] * uz
    return length(vx, vy, vz) < 0.35 and length(wx, wy, wz) < 0.3 and upright > tilt
end

--- How upright the lander stands: the cosine of the angle between its up axis and the local vertical.
---@pure
---@param hull entity
---@return number upright
function M.upright(hull)
    local x, y, z = transform.get_world_position(hull)
    local ux, uy, uz = unit(x, y, z)
    local _, up = axes(transform.get_rotation(hull))
    return up[1] * ux + up[2] * uy + up[3] * uz
end

--- Speed of the lander, in units per second.
---@param hull entity
---@return number speed
function M.speed(hull)
    return length(physics.get_velocity(hull))
end

--- Horizontal distance from the lander to the centre of a pad.
---@param hull entity
---@param pad entity
---@return number distance
function M.pad_distance(hull, pad)
    local x, y, z = transform.get_world_position(hull)
    local px, py, pz = transform.get_world_position(pad)
    local ux, uy, uz = unit(px, py, pz)
    local dx, dy, dz = x - px, y - py, z - pz
    local along = dx * ux + dy * uy + dz * uz
    return length(dx - ux * along, dy - uy * along, dz - uz * along)
end

--- How far the decks of the pads `Pad1`..`Pad<count>` sit off the ground: the largest gap, in metres, between the
--- bottom of a deck (`depth` below its centre) and the ground a metre past its rim (`half` from its centre), on its
--- four sides, along its own vertical; 0 when no pad is found.
---@param count integer
---@param half number
---@param depth number
---@return number gap
function M.pad_gap(count, half, depth)
    local worst = 0
    for index = 1, count do
        local pad = scene.find_entity("Pad" .. index)
        if pad ~= 0 then
            local px, py, pz = transform.get_world_position(pad)
            local x, y, z = axes(transform.get_rotation(pad))
            for _, side in ipairs({ { x, 1 }, { x, -1 }, { z, 1 }, { z, -1 } }) do
                local axis, sign = side[1], side[2] * (half + 1)
                local ox = px + axis[1] * sign + y[1] * 5
                local oy = py + axis[2] * sign + y[2] * 5
                local oz = pz + axis[3] * sign + y[3] * 5
                local hit = physics.raycast_3d(ox, oy, oz, -y[1], -y[2], -y[3], 20, GROUND)
                local gap = hit == nil and 20 or hit.distance - 5 - depth
                if math.abs(gap) > math.abs(worst) then worst = gap end
            end
        end
    end
    return worst
end

--- Give the hull and its legs the same velocity: the start of a landing.
---@param velocity vector3
function M.launch(velocity)
    local parts = { scene.find_entity("Lander") }
    for _, name in ipairs(LEGS) do parts[#parts + 1] = scene.find_entity(name) end
    for _, id in ipairs(parts) do
        if id ~= 0 then physics.set_velocity(id, velocity.x, velocity.y, velocity.z) end
    end
end

--- Show the predicted path: the `Bead1`..`Bead<count>` markers spread along the next seconds of flight until the
--- ground, the `ImpactMarker` where the path meets it; everything hidden when `visible` is false.
---@param hull entity
---@param count integer
---@param visible boolean
---@return boolean impact
function M.predict(hull, count, visible)
    local marker = scene.find_entity("ImpactMarker")
    local points = visible and physics.predict_trajectory(hull, 900) or {}
    local impact = nil
    local last = #points
    for i = 1, #points - 1 do
        local a, b = points[i], points[i + 1]
        if length(b.x, b.y, b.z) < 175 then
            local dx, dy, dz = b.x - a.x, b.y - a.y, b.z - a.z
            local step = length(dx, dy, dz)
            if step > 1e-6 then
                local hit = physics.raycast_3d(a.x, a.y, a.z, dx / step, dy / step, dz / step, step, GROUND)
                if hit ~= nil then
                    impact = hit
                    last = i
                    break
                end
            end
        end
    end
    for index = 1, count do
        local bead = scene.find_entity("Bead" .. index)
        if bead ~= 0 then
            local i = math.floor(index * last / (count + 1))
            local point = points[i]
            local show = point ~= nil and i > 8
            ui.set_visible(bead, show)
            if show then transform.set_position(bead, point.x, point.y, point.z) end
        end
    end
    if marker ~= 0 then
        ui.set_visible(marker, impact ~= nil)
        entity.set_hud_marker_enabled(marker, impact ~= nil)
        if impact ~= nil then
            transform.set_position(marker, impact.x, impact.y, impact.z)
            local ux, uy, uz = unit(impact.x, impact.y, impact.z)
            local fx, fy, fz = unit(cross(ux, uy, uz, 0, 0, 1))
            local bx, by, bz = cross(fx, fy, fz, ux, uy, uz)
            transform.set_rotation(marker, euler({ fx, fy, fz }, { ux, uy, uz }, { bx, by, bz }))
        end
    end
    return impact ~= nil
end

local camera_position = nil
-- The view the player turns: yaw about the local vertical from the reference heading, elevation above the horizon,
-- zoom on the chase distance; the reference heading is carried along the moon (kept in the tangent plane).
local view = { yaw = 0, pitch = 0.5, zoom = 1, ref = nil, mouse_x = nil, mouse_y = nil }
local ORBIT_RATE = 1.6 -- radians per second at full deflection of `camera_orbit`
local DRAG_RATE = 0.006 -- radians per pixel of a right-button drag
local ZOOM_RATE = 0.9 -- share of the distance per second of `camera_zoom`

-- Rodrigues rotation of a vector about a unit axis.
local function rotate(vx, vy, vz, ax, ay, az, angle)
    local c, s = math.cos(angle), math.sin(angle)
    local dot = vx * ax + vy * ay + vz * az
    local cx, cy, cz = cross(ax, ay, az, vx, vy, vz)
    return vx * c + cx * s + ax * dot * (1 - c), vy * c + cy * s + ay * dot * (1 - c), vz * c + cz * s + az * dot * (1 - c)
end

-- The orbit and the zoom of the view from the controls: `camera_orbit` (Q / E, R / F, the right stick), a drag with
-- the right mouse button, `camera_zoom` (Z / X, the bumpers) and the mouse wheel.
local function steer_view(dt)
    local ox, oy = input.get_action_vector("camera_orbit")
    view.yaw = view.yaw + ox * ORBIT_RATE * dt
    view.pitch = view.pitch - oy * ORBIT_RATE * dt
    local mx, my = input.get_mouse_x(), input.get_mouse_y()
    if input.is_action_pressed("camera_drag") and view.mouse_x ~= nil then
        local dx, dy = mx - view.mouse_x, my - view.mouse_y
        -- A jump of the cursor (a focus change) is not a drag.
        if math.abs(dx) < 200 and math.abs(dy) < 200 then
            view.yaw = view.yaw + dx * DRAG_RATE
            view.pitch = view.pitch + dy * DRAG_RATE
        end
    end
    view.mouse_x, view.mouse_y = mx, my
    local _, wheel = input.get_mouse_wheel()
    local zoom = input.get_action_value("camera_zoom") * ZOOM_RATE * dt - (wheel or 0) * 0.12
    view.zoom = math.max(0.45, math.min(3, view.zoom * (1 + zoom)))
    view.pitch = math.max(-0.15, math.min(1.45, view.pitch))
end

--- Chase camera: behind the lander along a heading carried with it over the moon, raised above its horizon, turned
--- and zoomed by the player; it eases after the lander and looks at it or, close to the ground, between the lander
--- and the ground under it. Returns the yaw, elevation and zoom of the view.
---@param camera entity
---@param hull entity
---@param distance number
---@param altitude number
---@param dt number
---@return number yaw
---@return number pitch
---@return number zoom
function M.camera(camera, hull, distance, altitude, dt)
    steer_view(dt)
    local x, y, z = transform.get_world_position(hull)
    local ux, uy, uz = unit(x, y, z)
    if view.ref == nil then
        -- The first heading is the lander's own forward axis on the horizon.
        local _, _, back = axes(transform.get_rotation(hull))
        view.ref = { -back[1], -back[2], -back[3] }
    end
    local rx0, ry0, rz0 = view.ref[1], view.ref[2], view.ref[3]
    local dot = rx0 * ux + ry0 * uy + rz0 * uz
    rx0, ry0, rz0 = rx0 - ux * dot, ry0 - uy * dot, rz0 - uz * dot
    if length(rx0, ry0, rz0) < 1e-3 then
        rx0, ry0, rz0 = cross(ux, uy, uz, 1, 0, 0)
    end
    rx0, ry0, rz0 = unit(rx0, ry0, rz0)
    view.ref = { rx0, ry0, rz0 }
    local fx, fy, fz = rotate(rx0, ry0, rz0, ux, uy, uz, view.yaw)
    local reach = distance * view.zoom
    local back_k, up_k = math.cos(view.pitch) * reach, math.sin(view.pitch) * reach
    local tx, ty, tz = x - fx * back_k + ux * up_k, y - fy * back_k + uy * up_k, z - fz * back_k + uz * up_k
    if camera_position == nil then camera_position = { tx, ty, tz } end
    local k = math.min(1, dt * 6)
    camera_position[1] = camera_position[1] + (tx - camera_position[1]) * k
    camera_position[2] = camera_position[2] + (ty - camera_position[2]) * k
    camera_position[3] = camera_position[3] + (tz - camera_position[3]) * k
    local cx, cy, cz = camera_position[1], camera_position[2], camera_position[3]
    transform.set_position(camera, cx, cy, cz)
    -- The camera looks along its -Z axis, its up near the local vertical.
    local drop = math.max(0, math.min(altitude * 0.4, 5))
    local ax, ay, az = x - ux * drop, y - uy * drop, z - uz * drop
    local lx, ly, lz = unit(ax - cx, ay - cy, az - cz)
    local zx, zy, zz = -lx, -ly, -lz
    local sx, sy, sz = unit(cross(ux, uy, uz, zx, zy, zz))
    local upx, upy, upz = cross(zx, zy, zz, sx, sy, sz)
    transform.set_rotation(camera, euler({ sx, sy, sz }, { upx, upy, upz }, { zx, zy, zz }))
    return view.yaw, view.pitch, view.zoom
end

--- Feet of the lander on the ground: a short ray from the sole of each leg still held, down the local vertical,
--- against the ground layer. Returns how many soles touch (within `margin`) and the lowest clearance of a sole.
---@param hull entity
---@param margin number
---@return integer contacts
---@return number clearance
function M.feet(hull, margin)
    local _, up = axes(transform.get_rotation(hull))
    local contacts, clearance = 0, math.huge
    for _, name in ipairs(LEGS) do
        local leg = scene.find_entity(name)
        if leg ~= 0 then
            -- The sole is half a leg below its centre, along the hull's up axis (the leg mesh is 1 unit tall).
            local lx, ly, lz = transform.get_world_position(leg)
            local sx, sy, sz = lx - up[1] * 0.5, ly - up[2] * 0.5, lz - up[3] * 0.5
            local ux, uy, uz = unit(sx, sy, sz)
            -- From a little above the sole, so a sole pressed into the ground still meets it.
            local hit = physics.raycast_3d(sx + ux * 0.2, sy + uy * 0.2, sz + uz * 0.2, -ux, -uy, -uz, 3, GROUND)
            if hit ~= nil then
                local gap = hit.distance - 0.2
                clearance = math.min(clearance, gap)
                if gap <= margin then contacts = contacts + 1 end
            end
        end
    end
    if clearance == math.huge then clearance = 3 end
    return contacts, clearance
end

--- Score of a landing: a base, the fuel left, the precision on the pad and the softness of the touchdown.
---@pure
---@param fuel number
---@param distance number
---@param impact number
---@return integer score
---@return integer precision
---@return integer softness
function M.score(fuel, distance, impact)
    local precision = math.max(0, math.floor(500 - distance * 160))
    local softness = math.max(0, math.floor(400 - impact * 160))
    return 1000 + math.floor(fuel * 10) + precision + softness, precision, softness
end

return M
