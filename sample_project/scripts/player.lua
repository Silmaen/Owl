-- Player controller
-- Demonstrates: input, physics, entity lookup, ui.set_text, scene.destroy_entity

properties = {
    { name = "speed",     type = "float", default = 8.0 },
    { name = "score",     type = "int",   default = 0 },
}

local score_text_id = 0

function on_create()
    log.info("Player created with speed=" .. speed)
    score_text_id = scene.find_entity("ScoreText")
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
                if score_text_id ~= 0 then
                    ui.set_text(score_text_id, "Score: " .. score)
                end
            end
        end
    end
end

function on_destroy()
    log.info("Player destroyed. Final score: " .. score)
end
