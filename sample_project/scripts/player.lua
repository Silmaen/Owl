-- Player controller
-- Demonstrates: input (keyboard + mouse), physics (impulse, get_velocity),
--               entity (has_component, get_name), ui (set_text, set_visible, set_progress),
--               scene (find_entity, destroy_entity, create_entity),
--               gamestate, save.save_game, settings, trigger enter/exit, health system,
--               victory/defeat flow, escape to menu, transform, time,
--               sound (play, stop, pause, resume, set_volume),
--               ui.transition_fade_in/out

properties = {
    { name = "speed",     type = "float", default = 8.0 },
    { name = "score",     type = "int",   default = 0 },
}

local score_text_id = 0
local health_bar_id = 0
local interact_prompt_id = 0
local music_status_id = 0
local health = 1.0
local initial_coin_count = 0
local coins_collected_this_level = 0
local escape_was_down = false
local jump_was_down = false
local mute_was_down = false
local music_handle = 0
local music_paused = false
local pending_scene = nil
local near_checkpoint = false

function on_create()
    -- Read player settings (game_settings.yml defaults, overridable by user).
    speed = settings.get("player_speed", speed)
    log.info("Player created with speed=" .. speed)

    -- Restore state from gamestate if continuing from a save or level transition
    local saved_score = gamestate.get("score", 0)
    if saved_score > 0 then
        score = saved_score
        log.info("Restored score from save: " .. score)
    end
    local saved_health = gamestate.get("health", 0)
    if saved_health > 0 then
        health = saved_health
        log.info("Restored health: " .. health)
    end

    score_text_id = scene.find_entity("ScoreText")
    health_bar_id = scene.find_entity("HealthBar")
    interact_prompt_id = scene.find_entity("InteractPrompt")
    music_status_id = scene.find_entity("MusicStatus")

    -- Count coins using entity.has_component to verify they are collectible.
    initial_coin_count = 0
    coins_collected_this_level = 0
    for i = 1, 99 do
        local coinId = scene.find_entity("Coin" .. i)
        if coinId ~= 0 then
            -- Verify this entity has a Trigger component (demonstrates entity.has_component)
            if entity.has_component(coinId, "Trigger") then
                local name = entity.get_name(coinId)
                log.trace("Found collectible: " .. name)
                initial_coin_count = initial_coin_count + 1
            end
        else
            break
        end
    end
    log.info("Level has " .. initial_coin_count .. " coins")

    update_hud()

    -- Start background music (looped) with volume from settings
    music_handle = sound.play("sounds/music.wav")
    local music_vol = settings.get("volume_master", 1.0) * 0.6
    sound.set_volume(music_handle, music_vol)

    -- Fade in when level starts
    ui.transition_fade_in(0.5)

    -- Create a marker entity to track session start (demonstrates scene.create_entity)
    scene.create_entity("SessionMarker")
    log.trace("Session marker created")
end

function update_hud()
    if score_text_id ~= 0 then
        ui.set_text(score_text_id, "Score: " .. score)
    end
    if health_bar_id ~= 0 then
        ui.set_progress(health_bar_id, math.max(0, health))
    end
end

function on_update(dt)
    -- Fade-out in progress: wait for transition then load scene
    if pending_scene then
        if not ui.is_transition_active() then
            scene.load_scene(pending_scene)
            pending_scene = nil
        end
        return
    end

    -- Movement (WASD: W=87, A=65, S=83, D=68)
    local moveX = 0
    local moveY = 0
    if input.is_key_pressed(87) then moveY = moveY + 1 end
    if input.is_key_pressed(83) then moveY = moveY - 1 end
    if input.is_key_pressed(65) then moveX = moveX - 1 end
    if input.is_key_pressed(68) then moveX = moveX + 1 end

    -- Jump sound (Space key edge detection)
    local jump_down = input.is_key_pressed(32)
    if jump_down and not jump_was_down then
        sound.play("sounds/jump.wav")
    end
    jump_was_down = jump_down

    if moveX ~= 0 or moveY ~= 0 then
        physics.impulse(entity_id, moveX * speed * dt, moveY * speed * dt)
    end

    -- Music toggle with M key (77) — demonstrates sound.pause / sound.resume
    local mute_down = input.is_key_pressed(77)
    if mute_down and not mute_was_down then
        if music_paused then
            sound.resume(music_handle)
            music_paused = false
            log.info("Music resumed")
            if music_status_id ~= 0 then
                ui.set_text(music_status_id, "Music: ON (M)")
            end
        else
            sound.pause(music_handle)
            music_paused = true
            log.info("Music paused")
            if music_status_id ~= 0 then
                ui.set_text(music_status_id, "Music: OFF (M)")
            end
        end
    end
    mute_was_down = mute_down

    -- Show/hide interact prompt near checkpoint (demonstrates ui.set_visible)
    local checkpoint_id = scene.find_entity("Checkpoint")
    if checkpoint_id ~= 0 and interact_prompt_id ~= 0 then
        local px, py, pz = transform.get_position(entity_id)
        local cx, cy, cz = transform.get_position(checkpoint_id)
        local dx = px - cx
        local dy = py - cy
        local dist = math.sqrt(dx * dx + dy * dy)
        local was_near = near_checkpoint
        near_checkpoint = dist < 2.0
        if near_checkpoint ~= was_near then
            ui.set_visible(interact_prompt_id, near_checkpoint)
        end
    end

    -- Health decreases over time (hazard simulation)
    health = health - dt * 0.04
    update_hud()

    -- Death: health reaches zero
    if health <= 0 then
        log.info("Player died! Health depleted.")
        sound.stop(music_handle)
        sound.play("sounds/death.wav")
        gamestate.set("score", score)
        pending_scene = "scenes/game_over.owl"
        ui.transition_fade_out(0.3)
        return
    end

    -- Victory: only if we collected all coins in this level AND there's no portal or victory zone.
    if coins_collected_this_level >= initial_coin_count and initial_coin_count > 0 then
        local has_portal = scene.find_entity("LevelPortal") ~= 0
        local has_victory_zone = scene.find_entity("VictoryZone") ~= 0
        if not has_portal and not has_victory_zone then
            log.info("All coins collected! Victory!")
            sound.stop(music_handle)
            sound.play("sounds/victory.wav")
            gamestate.set("score", score)
            save.save_game(1)
            pending_scene = "scenes/victory.owl"
            ui.transition_fade_out(0.3)
            return
        end
    end

    -- Coin collection (proximity-based)
    local x, y, z = transform.get_position(entity_id)
    for i = 1, initial_coin_count do
        local coinName = "Coin" .. i
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
                coins_collected_this_level = coins_collected_this_level + 1
                gamestate.set("score", score)
                -- Heal on coin pickup
                health = math.min(1.0, health + 0.3)
                gamestate.set("health", health)
                update_hud()
            end
        end
    end

    -- Persist health in gamestate each frame (for level transitions)
    gamestate.set("health", health)

    -- Quick-save with F5 (294)
    if input.is_key_pressed(294) then
        gamestate.set("score", score)
        save.save_game(1)
        log.info("Quick-saved to slot 1")
    end

    -- Escape to main menu (256 = Escape)
    local escape_down = input.is_key_pressed(256)
    if escape_down and not escape_was_down then
        log.info("Escape pressed — returning to main menu")
        sound.stop(music_handle)
        gamestate.set("score", score)
        pending_scene = "scenes/main_menu.owl"
        ui.transition_fade_out(0.3)
    end
    escape_was_down = escape_down
end

-- Trigger edge callbacks
function on_trigger_enter(other_id)
    log.info("Player entered trigger zone")
end

function on_trigger_exit(other_id)
    log.trace("Player exited trigger zone")
end

function on_destroy()
    log.info("Player destroyed. Final score: " .. score)
end
