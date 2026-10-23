-- The neon brick wall behind the Breakout menus (script of the `NeonWall` entity, its children are the bricks).
--
-- A wave of light runs along the rows: each brick keeps the colour it is authored with and is lit from 30 to 100 %.
-- On the title a ball and a paddle play by themselves (`MenuBall`, `MenuPaddle`): a brick the ball hits goes dark and
-- comes back a few seconds later with a flash. Nothing here reads the input, the menus stay in front.

properties = {
    { name = "wave_speed", type = "float", default = 2.4, min = 0 },
    { name = "wave_length", type = "float", default = 0.45, min = 0.01 },
    { name = "ball_speed", type = "float", default = 7.5, min = 0 },
    { name = "respawn_time", type = "float", default = 4.0, min = 0 },
}

local BALL_HALF = 0.15
local BRICK_HALF_W, BRICK_HALF_H = 0.75, 0.25
local X_LIMIT, FLOOR_Y, CEILING_Y = 9.4, -6.85, 6.6

local bricks = {}
local ball, paddle = 0, 0
local bx, by, vx, vy = 0.0, -6.0, 0.6, 0.8
local px = 0.0
local clock = 0.0

function on_create()
    for _, id in ipairs(scene.find_entities("MenuBrick")) do
        local x, y = transform.get_position(id)
        local r, g, b = entity.get_tint(id)
        bricks[#bricks + 1] = { id = id, x = x, y = y, r = r or 1, g = g or 1, b = b or 1, down = 0.0, lit = -1 }
    end
    ball = scene.find_entity("MenuBall")
    paddle = scene.find_entity("MenuPaddle")
    local norm = math.sqrt(vx * vx + vy * vy)
    vx, vy = vx / norm * ball_speed, vy / norm * ball_speed
end

local function light(brick, level)
    -- Rounded so that a brick whose light did not change costs no call.
    local step = math.floor(level * 20 + 0.5)
    if step ~= brick.lit then
        brick.lit = step
        local k = step / 20
        entity.set_tint(brick.id, brick.r * k, brick.g * k, brick.b * k, 1.0)
    end
end

local function hit_brick()
    for _, brick in ipairs(bricks) do
        if brick.down <= 0 and math.abs(bx - brick.x) < BRICK_HALF_W + BALL_HALF and
                math.abs(by - brick.y) < BRICK_HALF_H + BALL_HALF then
            brick.down = respawn_time
            ui.set_visible(brick.id, false)
            return true
        end
    end
    return false
end

local function play(dt)
    bx, by = bx + vx * dt, by + vy * dt
    if math.abs(bx) > X_LIMIT then
        bx = X_LIMIT * (bx > 0 and 1 or -1)
        vx = -vx
    end
    if by > CEILING_Y then
        by = CEILING_Y
        vy = -math.abs(vy)
    elseif vy > 0 and hit_brick() then
        vy = -vy
    elseif by < FLOOR_Y then
        -- The paddle is always there: the angle depends on where the ball lands on it.
        by = FLOOR_Y
        local angle = math.max(-0.9, math.min(0.9, (bx - px) * 0.8))
        vx, vy = math.sin(angle) * ball_speed, math.cos(angle) * ball_speed
    end
    -- The paddle follows the ball a little late, as a player would.
    px = px + (bx - px) * math.min(1.0, dt * 6.0)
    transform.set_position(ball, bx, by, 0.2)
    transform.set_position(paddle, px, FLOOR_Y - 0.25, 0.1)
end

function on_update(dt)
    clock = clock + dt
    for _, brick in ipairs(bricks) do
        if brick.down > 0 then
            brick.down = brick.down - dt
            if brick.down <= 0 then
                ui.set_visible(brick.id, true)
                brick.lit = -1
                light(brick, 1.0)
            end
        else
            local wave = 0.5 + 0.5 * math.cos(brick.x * wave_length - clock * wave_speed + brick.y * 0.6)
            light(brick, 0.3 + 0.7 * wave * wave * wave)
        end
    end
    if ball ~= 0 and paddle ~= 0 then
        play(math.min(dt, 0.05))
    end
end
