-- Coin collectible
-- Demonstrates: trigger callbacks (LuaCallback type), entity destruction, gamestate, sound.play, and a pick-up pop
--               tweened from on_update (scale up, tint to a bright flash then fade, a small rise) before the coin is
--               destroyed. The spin is the sprite animation: two frames of the platformer atlas.

properties = {
    { name = "pop_time", type = "float", default = 0.28 },  -- s of the pop
    { name = "pop_scale", type = "float", default = 1.9 },  -- scale reached at the end of the pop
    { name = "pop_rise", type = "float", default = 0.7 },  -- world units the coin rises
}

local collected = false
local pop_clock = 0.0
local home_x, home_y, home_z = 0.0, 0.0, 0.0
local base_scale = 1.0

function on_create()
    log.trace("Coin spawned")
    base_scale = transform.get_scale(entity_id)
end

-- Called by the LuaCallback trigger when the player overlaps (every frame while it does)
function on_coin_collected()
    if collected then return end
    collected = true
    home_x, home_y, home_z = transform.get_position(entity_id)

    local current_score = gamestate.get("score", 0)
    current_score = current_score + 10
    gamestate.set("score", current_score)
    log.info("Coin collected! Score: " .. current_score)
    sound.play("sounds/platformer/coin_collect.wav")

    -- Update HUD
    local score_text = scene.find_entity("ScoreText")
    if score_text ~= 0 then
        ui.set_text(score_text, "Score: " .. current_score)
    end
end

-- The pop: ease-out progress t in [0, 1], destroy the coin when it is over.
function on_update(dt)
    if not collected then return end
    pop_clock = pop_clock + dt
    local t = math.min(1.0, pop_clock / pop_time)
    local eased = 1.0 - (1.0 - t) * (1.0 - t)
    local scale = base_scale * (1.0 + (pop_scale - 1.0) * eased)
    transform.set_scale(entity_id, scale, scale, 1)
    transform.set_position(entity_id, home_x, home_y + pop_rise * eased, home_z)
    entity.set_tint(entity_id, 1.0, 1.0, 1.0 - 0.5 * (1.0 - t), 1.0 - t * t)
    if t >= 1.0 then
        scene.destroy_entity(entity_id)
    end
end

function on_trigger_enter()
    log.trace("Player near coin")
end

function on_destroy()
    log.trace("Coin destroyed")
end
