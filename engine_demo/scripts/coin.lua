-- Coin collectible
-- Demonstrates: trigger callbacks (LuaCallback type), entity destruction,
--               gamestate, transform rotation animation, sound.play

local spin_speed = 2.0

function on_create()
    log.trace("Coin spawned")
end

function on_update(dt)
    -- Gentle spin animation
    local rx, ry, rz = transform.get_rotation(entity_id)
    transform.set_rotation(entity_id, rx, ry, rz + spin_speed * dt)
end

-- Called by the LuaCallback trigger when the player overlaps
function on_coin_collected()
    local current_score = gamestate.get("score", 0)
    current_score = current_score + 10
    gamestate.set("score", current_score)
    log.info("Coin collected! Score: " .. current_score)
    sound.play("sounds/coin_collect.wav")

    -- Update HUD
    local score_text = scene.find_entity("ScoreText")
    if score_text ~= 0 then
        ui.set_text(score_text, "Score: " .. current_score)
    end

    -- Destroy this coin
    scene.destroy_entity(entity_id)
end

function on_trigger_enter()
    log.trace("Player near coin")
end

function on_destroy()
    log.trace("Coin destroyed")
end
