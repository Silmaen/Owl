-- The paddle: the `paddle` action (A / D, arrows, left stick, D-pad) or the mouse moves it along the bottom of the
-- field. A kinematic body driven by its velocity, so that Box2D sees it move and the ball bounces off it.
--
-- The wide power-up swaps it for `PaddleWide`, a second authored paddle parked below the field: a body cannot be
-- resized from Lua, so the game keeps one body per width (a pool, gap G1). The mouse moves the paddle by its motion
-- (gap G5: no screen-to-world conversion in Lua), scaled by the `breakout_mouse_speed` setting.
-- Publishes `breakout_paddle_x` and `breakout_paddle_half` for the ball and the power-ups.

properties = {
    { name = "speed", type = "float", default = 13.0 },
    { name = "half_width", type = "float", default = 1.2 },
    { name = "wide_half_width", type = "float", default = 1.8 },
    { name = "field_half", type = "float", default = 8.0 },   -- inner half width of the field (walls at +-8)
    { name = "paddle_y", type = "float", default = -6.8 },   -- height of the paddle
    { name = "park_y", type = "float", default = -30.0 },   -- where the unused paddle waits (`PaddleWide` is authored there)
}

local FIELD_HALF, PADDLE_Y, PARK_Y    -- set from the properties in on_create
local UNITS_PER_PIXEL = 16.0 / 720.0  -- the camera shows 16 units over 720 pixels at the reference size

local wide_id = 0
local active_id = 0
local wide = false
local x = 0.0
local last_mouse_x = 0.0
local stopped = false

local function half()
    return wide and wide_half_width or half_width
end

local function publish()
    gamestate.set("breakout_paddle_x", x)
    gamestate.set("breakout_paddle_half", half())
end

function on_create()
    FIELD_HALF = field_half
    PADDLE_Y = paddle_y
    PARK_Y = park_y
    wide_id = scene.find_entity("PaddleWide")
    active_id = entity_id
    x = transform.get_position(entity_id)
    last_mouse_x = input.get_mouse_x()
    publish()
end

local function swap(to_wide)
    if wide_id == 0 then return end
    physics.set_velocity(active_id, 0.0, 0.0)
    physics.set_transform(active_id, 0.0, PARK_Y, 0.0)
    wide = to_wide
    active_id = wide and wide_id or entity_id
    x = math.max(-FIELD_HALF + half(), math.min(FIELD_HALF - half(), x))
    physics.set_transform(active_id, x, PADDLE_Y, 0.0)
end

function on_tick(dt)
    local mouse_x = input.get_mouse_x()
    local mouse_dx = mouse_x - last_mouse_x
    last_mouse_x = mouse_x
    if gamestate.get("breakout_paused", false) or gamestate.get("breakout_frozen", false) then
        if not stopped then
            physics.set_velocity(active_id, 0.0, 0.0)
            stopped = true
        end
        return
    end
    stopped = false
    if gamestate.get("breakout_wide", false) ~= wide then
        swap(not wide)
    end
    local move = input.get_action_value("paddle") * speed * dt
    if not ui.is_mouse_over_ui() then
        move = move + mouse_dx * UNITS_PER_PIXEL * settings.get("breakout_mouse_speed", 1.0)
    end
    local target = math.max(-FIELD_HALF + half(), math.min(FIELD_HALF - half(), x + move))
    physics.set_velocity(active_id, (target - x) / dt, 0.0)
    x = target
    publish()
end
