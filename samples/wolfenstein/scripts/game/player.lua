-- The player of Wolfenstein mini: walk and turn, Wolfenstein 3D style (no strafe).
-- `walk` (W / S, arrows, left stick, D-pad) moves along the view, `turn` (A / D, arrows, sticks, D-pad) turns it;
-- `run` (Shift, a stick click or the right bumper) is faster, and the mouse turns the view while its right button is
-- held. Doors, secret walls and the elevator answer `interact` (E, Space, left click, pad A or X) by themselves.
--
-- Convention of the raycaster: rotation [0, 0, rz] in radians, forward = (-sin(rz), cos(rz)), rz = 0 faces north.
-- The body is the source of truth of a physics entity: the turn goes through `physics.set_transform`.

properties = {
    { name = "speed", type = "float", default = 3.0 },
    { name = "turn_speed", type = "float", default = 1.8 },  -- radians per second
    { name = "mouse_turn", type = "float", default = 0.006 }, -- radians per mouse pixel, times `mouse_sensitivity`
    { name = "run_factor", type = "float", default = 1.7, min = 1 },
}


-- The yaw lives here: the transform only follows the body on the fixed physics tick, so reading it back every frame
-- would drop or repeat a turn whenever frames and ticks do not line up.
local yaw = 0
local last_mouse_x = nil

function on_create()
    physics.set_gravity_scale(entity_id, 0)
    local _, _, start_yaw = transform.get_rotation(entity_id)
    yaw = start_yaw
end

function on_update(dt)
    if ui.is_transition_active() or gamestate.get("wolf_leaving", 0) == 1 or gamestate.get("wolf_paused", false) then
        physics.set_velocity(entity_id, 0, 0)
        return
    end

    local turn = input.get_action_value("turn") * turn_speed * dt
    local mouse_x = input.get_mouse_x()
    if input.is_action_pressed("mouse_look") and last_mouse_x ~= nil then
        turn = turn - (mouse_x - last_mouse_x) * mouse_turn * settings.get("mouse_sensitivity", 1.0)
    end
    last_mouse_x = mouse_x
    local px, py = transform.get_position(entity_id)
    if turn ~= 0 then
        yaw = (yaw + turn + math.pi) % (2 * math.pi) - math.pi
        physics.set_transform(entity_id, px, py, yaw)
    end

    local walk = input.get_action_value("walk") * speed
    if input.is_action_pressed("run") then
        walk = walk * run_factor
    end
    physics.set_velocity(entity_id, -math.sin(yaw) * walk, math.cos(yaw) * walk)
end
