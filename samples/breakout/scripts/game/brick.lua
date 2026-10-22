-- A brick (`prefabs/brick.owlprefab`): `hits` touches of the ball break it for `points`. A brick that holds flashes
-- white and darkens from steel (3) to silver (2) to its own colour (`r`, `g`, `b`, the last hit). A broken brick adds its points to `breakout_score`, takes one off
-- `breakout_bricks` and queues an event for `game.lua` in `breakout_events` (its burst, its power-up when `drop` is
-- `wide` or `slow`), then is destroyed.

properties = {
    { name = "hits", type = "int", default = 1 },
    { name = "points", type = "int", default = 10 },
    { name = "drop", type = "string", default = "" },
    { name = "r", type = "float", default = 0.9 },
    { name = "g", type = "float", default = 0.3 },
    { name = "b", type = "float", default = 0.3 },
}

local FLASH_TIME = 0.12

local remaining = 1
local broken = false
local flash = 0.0

local function colour()
    if remaining >= 3 then return 0.55, 0.6, 0.72 end
    if remaining == 2 then return 0.82, 0.85, 0.92 end
    return r, g, b
end

local function apply_tint()
    local cr, cg, cb = colour()
    local k = flash / FLASH_TIME
    entity.set_tint(entity_id, cr + (1.0 - cr) * k, cg + (1.0 - cg) * k, cb + (1.0 - cb) * k, 1.0)
end

function on_create()
    remaining = math.max(1, hits)
    apply_tint()
end

local function shatter()
    broken = true
    gamestate.set("breakout_score", gamestate.get("breakout_score", 0) + points)
    gamestate.set("breakout_bricks", gamestate.get("breakout_bricks", 1) - 1)
    local x, y = transform.get_position(entity_id)
    local cr, cg, cb = colour()
    local event = string.format("break %.3f %.3f %.3f %.3f %.3f;", x, y, cr, cg, cb)
    if drop == "wide" or drop == "slow" then
        event = event .. string.format("%s %.3f %.3f 0 0 0;", drop, x, y)
    end
    gamestate.set("breakout_events", gamestate.get("breakout_events", "") .. event)
    sound.play("sounds/game/brick_break.wav")
    scene.destroy_entity(entity_id)
end

function on_collision(other_id)
    if broken or entity.get_name(other_id) ~= "Ball" then return end
    remaining = remaining - 1
    if remaining <= 0 then
        shatter()
        return
    end
    flash = FLASH_TIME
    apply_tint()
    sound.play("sounds/game/metal.wav")
end

function on_update(dt)
    if flash <= 0.0 then return end
    flash = math.max(0.0, flash - dt)
    apply_tint()
end
