-- The camera over the level, its axes, the billboards and the projection of world points on the screen.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local Cam = td.camera

function Cam.update_basis()
    local cy, sy = math.cos(S.view.yaw), math.sin(S.view.yaw)
    local cp, sp = math.cos(camera_pitch), math.sin(camera_pitch)
    S.basis.right = { cy, 0.0, -sy }
    S.basis.up = { sp * sy, cp, sp * cy }
    S.basis.forward = { -cp * sy, sp, -cp * cy }
    S.basis.toward = { sy, cy }       -- toward the camera on the ground plane
end

function Cam.place_camera()
    if S.camera_id == 0 then return end
    Cam.update_basis()
    local f = S.basis.forward
    local target_y = C.GRASS_TOP
    local view = S.view
    view.eye = { view.x - f[1] * view.distance, target_y - f[2] * view.distance, view.z - f[3] * view.distance }
    transform.set_position(S.camera_id, S.view.eye[1], S.view.eye[2], S.view.eye[3])
    transform.set_rotation(S.camera_id, camera_pitch, S.view.yaw, 0.0)
end

-- The centre of a billboard whose foot (22 pixels under the cell centre) stands at a world point.
function Cam.billboard_at(x, y, z, lift)
    local up, f = S.basis.up, S.basis.forward
    lift = lift or S.sprite_lift
    return x + up[1] * lift - f[1] * C.TOWARD_VIEW, y + up[2] * lift - f[2] * C.TOWARD_VIEW,
            z + up[3] * lift - f[3] * C.TOWARD_VIEW
end

-- The angle on the screen of a world direction, to turn an arrow along its flight.
function Cam.screen_roll(dx, dy, dz)
    local r, u = S.basis.right, S.basis.up
    return math.atan(dx * u[1] + dy * u[2] + dz * u[3], dx * r[1] + dy * r[2] + dz * r[3])
end

-- The pixel of a world point and the world size of a pixel there, through the camera.
function Cam.project(x, y, z)
    local width, height = scene.get_viewport_size()
    if width == nil or width <= 0 then width, height = 1280, 720 end
    local dx, dy, dz = x - S.view.eye[1], y - S.view.eye[2], z - S.view.eye[3]
    local f, r, u = S.basis.forward, S.basis.right, S.basis.up
    local depth = math.max(0.1, dx * f[1] + dy * f[2] + dz * f[3])
    local half = math.tan(camera_fov * 0.5)
    local aspect = width / height
    local sx = (dx * r[1] + dy * r[2] + dz * r[3]) / (depth * half * aspect)
    local sy = (dx * u[1] + dy * u[2] + dz * u[3]) / (depth * half)
    return (sx + 1.0) * 0.5 * width, (1.0 - sy) * 0.5 * height, 2.0 * depth * half / height, width, height
end

-- A world point moved by pixels on the screen (right, up) at the depth of a point.
function Cam.screen_offset(x, y, z, px, py, unit, behind)
    local r, u, f = S.basis.right, S.basis.up, S.basis.forward
    behind = behind or 0.0
    return x + (r[1] * px + u[1] * py) * unit + f[1] * behind, y + (r[2] * px + u[2] * py) * unit + f[2] * behind,
            z + (r[3] * px + u[3] * py) * unit + f[3] * behind
end

function Cam.update_camera(real_dt)
    if S.camera_id == 0 then return end
    local right, down = input.get_action_vector("pan")
    local width, height = scene.get_viewport_size()
    if S.mouse_moved and width > 0 and edge_pixels > 0 and not ui.is_mouse_over_ui() then
        local mx, my = scene.get_viewport_mouse()
        if mx >= 0 and my >= 0 and mx < width and my < height then
            if mx < edge_pixels then right = -1 elseif mx > width - edge_pixels then right = 1 end
            if my < edge_pixels then down = -1 elseif my > height - edge_pixels then down = 1 end
        end
    end
    local _, wheel = input.get_mouse_wheel()
    local zoom = input.get_action_value("zoom") * real_dt * 12.0 - wheel * 1.2
    S.view.distance = math.max(zoom_min, math.min(zoom_max, S.view.distance + zoom))
    if math.abs(S.view.yaw_goal - S.view.yaw) > 1e-4 then
        local delta = S.view.yaw_goal - S.view.yaw
        local turn = math.min(1.0, real_dt * 8.0)
        S.view.yaw = math.abs(delta) < 0.002 and S.view.yaw_goal or S.view.yaw + delta * turn
    end
    Cam.update_basis()
    if S.pad_mode and S.cursor_pad ~= nil then
        -- The keyboard and gamepad cursor drags the view along when it leaves the middle of the screen.
        local dx, dz = S.cursor_pad.x - S.view.x, S.cursor_pad.z - S.view.z
        if math.abs(dx) > 3.5 or math.abs(dz) > 2.5 then
            S.view.x = S.view.x + dx * math.min(1.0, real_dt * 3.0)
            S.view.z = S.view.z + dz * math.min(1.0, real_dt * 3.0)
        end
    end
    if right ~= 0 or down ~= 0 then
        -- Screen right is the camera right on the ground, screen down is toward the camera.
        local step = pan_speed * real_dt * S.view.distance / camera_distance
        local r, t = S.basis.right, S.basis.toward
        S.view.x = S.view.x + (right * r[1] + down * t[1]) * step
        S.view.z = S.view.z + (right * r[3] + down * t[2]) * step
    end
    S.view.x = math.max(0.0, math.min(map_width - 1, S.view.x))
    S.view.z = math.max(0.0, math.min(map_depth - 1, S.view.z))
    Cam.place_camera()
end

return Cam
