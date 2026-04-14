-- Player controller
-- Demonstrates: input, physics, entity lookup, ui.set_text, scene.destroy_entity,
--               gamestate, save.save_game

properties = {
    { name = "speed",     type = "float", default = 8.0 },
    { name = "score",     type = "int",   default = 0 },
}

local score_text_id = 0
local total_coins = 3

function on_create()
    -- Read player settings (game_settings.yml defaults, overridable by user).
    speed = settings.get("player_speed", speed)
    local jump = settings.get("player_jump_impulse", 12.0)
    log.info("Player speed=" .. speed .. " jump=" .. jump)

    -- Restore score from gamestate if continuing from a save
    local saved_score = gamestate.get("score", 0)
    if saved_score > 0 then
        score = saved_score
        log.info("Player restored from save with score=" .. score)
    else
        log.info("Player created with speed=" .. speed)
    end
    score_text_id = scene.find_entity("ScoreText")
    if score_text_id ~= 0 then
        ui.set_text(score_text_id, "Score: " .. score)
    end
end

function on_update(dt)
    local moveX = 0
    local moveY = 0

    -- WASD movement (key codes: W=87, A=65, S=83, D=68)
    if input.is_key_pressed(87) then moveY = moveY + 1 end
    if input.is_key_pressed(83) then moveY = moveY - 1 end
    if input.is_key_pressed(65) then moveX = moveX - 1 end
    if input.is_key_pressed(68) then moveX = moveX + 1 end

    if moveX ~= 0 or moveY ~= 0 then
        physics.impulse(entity_id, moveX * speed * dt, moveY * speed * dt)
    end

    local x, y, z = transform.get_position(entity_id)

    -- Collect coins on proximity
    local coins = {"Coin1", "Coin2", "Coin3"}
    for _, coinName in ipairs(coins) do
        local coinId = scene.find_entity(coinName)
        if coinId ~= 0 then
            local cx, cy, cz = transform.get_position(coinId)
            local dx = x - cx
            local dy = y - cy
            local dist = math.sqrt(dx * dx + dy * dy)
            if dist < 0.8 then
                score = score + 10
                log.info("Collected " .. coinName .. "! Score: " .. score)
                scene.destroy_entity(coinId)
                -- Store score in gamestate (persists across saves)
                gamestate.set("score", score)
                if score_text_id ~= 0 then
                    ui.set_text(score_text_id, "Score: " .. score)
                end
                -- Auto-save when all coins collected
                if score >= total_coins * 10 then
                    log.info("All coins collected! Auto-saving to slot 1...")
                    save.save_game(1)
                end
            end
        end
    end

    -- Quick-save with F5
    if input.is_key_pressed(294) then  -- F5
        gamestate.set("score", score)
        save.save_game(1)
        log.info("Quick-saved to slot 1")
    end
end

function on_destroy()
    log.info("Player destroyed. Final score: " .. score)
end
