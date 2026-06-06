-- Free-fly camera for the voxel demo.
--
-- WASD moves in the camera's facing plane, Space/E rises and Shift/Q descends,
-- and the arrow keys look around (yaw/pitch). Pitch is clamped so you can't flip
-- over the poles. Movement is frame-rate independent (scaled by dt). Attach this
-- to the scene's perspective camera so you can explore the voxel world in Play —
-- the editor viewport already has its own orbit camera.

properties = {
    { name = "move_speed", type = "float", default = 8.0 },
    { name = "look_speed", type = "float", default = 1.5 },
}

-- GLFW key codes (see input/KeyCodes.h).
local KEY_SPACE = 32
local KEY_A, KEY_D, KEY_E, KEY_Q, KEY_S, KEY_W = 65, 68, 69, 81, 83, 87
local KEY_RIGHT, KEY_LEFT, KEY_DOWN, KEY_UP = 262, 263, 264, 265
local KEY_LSHIFT = 340

local k_pitch_limit = 1.5  -- rad, just under 90 deg

local yaw = 0.0
local pitch = 0.0

function on_create()
    local px, py, _ = transform.get_rotation(entity_id)
    pitch = px
    yaw = py
    log.info("Voxel fly camera ready (WASD move, arrows look, Space/Shift up/down)")
end

function on_update(dt)
    if input.is_key_pressed(KEY_LEFT) then yaw = yaw - look_speed * dt end
    if input.is_key_pressed(KEY_RIGHT) then yaw = yaw + look_speed * dt end
    if input.is_key_pressed(KEY_UP) then pitch = pitch + look_speed * dt end
    if input.is_key_pressed(KEY_DOWN) then pitch = pitch - look_speed * dt end
    if pitch > k_pitch_limit then pitch = k_pitch_limit end
    if pitch < -k_pitch_limit then pitch = -k_pitch_limit end
    transform.set_rotation(entity_id, pitch, yaw, 0.0)

    local cp = math.cos(pitch)
    local fx, fy, fz = cp * math.sin(yaw), math.sin(pitch), -cp * math.cos(yaw)
    local rx, rz = math.cos(yaw), math.sin(yaw)

    local dx, dy, dz = 0.0, 0.0, 0.0
    if input.is_key_pressed(KEY_W) then dx = dx + fx; dy = dy + fy; dz = dz + fz end
    if input.is_key_pressed(KEY_S) then dx = dx - fx; dy = dy - fy; dz = dz - fz end
    if input.is_key_pressed(KEY_D) then dx = dx + rx; dz = dz + rz end
    if input.is_key_pressed(KEY_A) then dx = dx - rx; dz = dz - rz end
    if input.is_key_pressed(KEY_SPACE) or input.is_key_pressed(KEY_E) then dy = dy + 1.0 end
    if input.is_key_pressed(KEY_LSHIFT) or input.is_key_pressed(KEY_Q) then dy = dy - 1.0 end

    if dx ~= 0.0 or dy ~= 0.0 or dz ~= 0.0 then
        local px, py, pz = transform.get_position(entity_id)
        local step = move_speed * dt
        transform.set_position(entity_id, px + dx * step, py + dy * step, pz + dz * step)
    end
end
