-- The ball: a dynamic body without gravity, bouncing with a restitution of 1 and no friction. Box2D solves the bounces;
-- this script keeps the speed constant (`physics.set_velocity` every tick), aims the bounce off the paddle by where the
-- ball lands on it, and handles the serve and the lost balls.
--
-- Serve: the ball rides on the paddle until the `serve` action (Space, pad A, left click), or by itself after
-- `auto_serve` seconds. A ball below the field costs a life (`breakout_lives`). The speed grows a little at each
-- paddle bounce up to `max_speed`, and follows `breakout_speed_factor` (the slow power-up).

properties = {
    { name = "speed", type = "float", default = 7.5 },
    { name = "max_speed", type = "float", default = 11.0 },
    { name = "auto_serve", type = "float", default = 5.0 },
}

local PADDLE_Y = -6.8     -- height of the paddle
local RIDE_HEIGHT = 0.36  -- height of the ball above the paddle centre before the serve
local LOST_Y = -8.6       -- below this the ball is lost
local MAX_ANGLE = 1.05    -- largest bounce angle off the paddle, from the vertical (about 60 degrees)
local MIN_VERTICAL = 0.3  -- smallest share of the speed kept vertical, so the ball never runs flat

local state = "serve"     -- "serve", "play" or "out" (no life left)
local serve_clock = 0.0
local current_speed = 0.0
local held_vx, held_vy = 0.0, 0.0
local held = false
local serve_guard = 0.0   -- seconds after a pause when the serve is ignored (pad A also clicks Resume)

local function set_state(value)
    state = value
    gamestate.set("breakout_ball_state", value)
end

function on_create()
    physics.set_gravity_scale(entity_id, 0.0)
    current_speed = speed
    set_state("serve")
end

local function launch()
    local angle = 0.35
    if gamestate.get("breakout_serves", 0) % 2 == 1 then angle = -angle end
    gamestate.set("breakout_serves", gamestate.get("breakout_serves", 0) + 1)
    local s = current_speed * gamestate.get("breakout_speed_factor", 1.0)
    physics.set_velocity(entity_id, math.sin(angle) * s, math.cos(angle) * s)
    sound.play("sounds/game/serve.wav")
    set_state("play")
end

local function ride_paddle(dt)
    local px = gamestate.get("breakout_paddle_x", 0.0)
    physics.set_transform(entity_id, px, PADDLE_Y + RIDE_HEIGHT, 0.0)
    physics.set_velocity(entity_id, 0.0, 0.0)
    serve_clock = serve_clock + dt
    serve_guard = math.max(0.0, serve_guard - dt)
    local pressed = serve_guard <= 0.0 and input.is_action_just_pressed("serve")
    if pressed or serve_clock >= auto_serve then
        launch()
    end
end

local function lose_ball()
    sound.play("sounds/game/lose.wav")
    local lives = gamestate.get("breakout_lives", 3) - 1
    gamestate.set("breakout_lives", lives)
    gamestate.set("breakout_wide", false)
    gamestate.set("breakout_speed_factor", 1.0)
    current_speed = speed
    serve_clock = 0.0
    if lives <= 0 then
        physics.set_velocity(entity_id, 0.0, 0.0)
        physics.set_transform(entity_id, 0.0, -30.0, 0.0)
        set_state("out")
    else
        set_state("serve")
    end
end

-- Constant speed, never flat.
local function keep_speed()
    local vx, vy = physics.get_velocity(entity_id)
    local s = current_speed * gamestate.get("breakout_speed_factor", 1.0)
    local length = math.sqrt(vx * vx + vy * vy)
    if length < 1e-4 then
        vx, vy = 0.0, 1.0
        length = 1.0
    end
    vx, vy = vx / length, vy / length
    if math.abs(vy) < MIN_VERTICAL then
        vy = (vy < 0.0) and -MIN_VERTICAL or MIN_VERTICAL
        vx = (vx < 0.0 and -1.0 or 1.0) * math.sqrt(1.0 - MIN_VERTICAL * MIN_VERTICAL)
    end
    physics.set_velocity(entity_id, vx * s, vy * s)
end

function on_tick(dt)
    if gamestate.get("breakout_paused", false) or gamestate.get("breakout_frozen", false) then
        if not held then
            held_vx, held_vy = physics.get_velocity(entity_id)
            physics.set_velocity(entity_id, 0.0, 0.0)
            held = true
        end
        return
    end
    if held then
        physics.set_velocity(entity_id, held_vx, held_vy)
        held = false
        serve_guard = 0.25
    end
    if state == "serve" then
        ride_paddle(dt)
    elseif state == "play" then
        local _, y = transform.get_position(entity_id)
        if y < LOST_Y then
            lose_ball()
        else
            keep_speed()
        end
    end
end

function on_collision(other_id)
    if state ~= "play" then return end
    local name = entity.get_name(other_id)
    if name == "Paddle" or name == "PaddleWide" then
        local bx = transform.get_position(entity_id)
        local px = gamestate.get("breakout_paddle_x", 0.0)
        local half = gamestate.get("breakout_paddle_half", 1.2)
        local offset = math.max(-1.0, math.min(1.0, (bx - px) / half))
        current_speed = math.min(max_speed, current_speed + 0.12)
        local s = current_speed * gamestate.get("breakout_speed_factor", 1.0)
        physics.set_velocity(entity_id, math.sin(offset * MAX_ANGLE) * s, math.cos(offset * MAX_ANGLE) * s)
        sound.play("sounds/game/paddle.wav")
    elseif name == "Wall" then
        sound.play("sounds/game/wall.wav")
    end
end
