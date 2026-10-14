-- Platformer player controller — tight controls + natural feel.
--
-- Ground detection is a small state machine (grounded / rising / falling)
-- with permissive vertical-velocity thresholds, so a kinematic moving
-- platform that pushes the player at ±1.5 still counts as ground.  A short
-- coyote time keeps the jump available for a few frames after walking off
-- a ledge, and a jump buffer fires the jump on landing if the user pressed
-- it just before touchdown.  These two together remove the "press exactly
-- when grounded" frustration without changing the no-air-control rule:
-- once airborne, A/D do nothing — your horizontal momentum is locked.
-- Gamepad 0 plays too: left stick or D-pad to walk, A to jump, Start to pause.

properties = {
    { name = "speed", type = "float", default = 5.5 },
}

-- Tunables ---------------------------------------------------------------------
local k_jump_speed     = 7.5    -- absolute upward velocity when jump fires
local k_coyote_time    = 0.18   -- s of grace after walking off a ledge
local k_jump_buffer    = 0.15   -- s before landing that a jump press is remembered
local k_rise_threshold = 3.0    -- vy above this = "rising" (jumped or rocketed up)
local k_fall_threshold = 3.0    -- vy below -threshold = "falling" (or walked off)
local k_apex_threshold = 0.5    -- vy below this from rising = transition to falling
local k_land_threshold = 0.5    -- vy above -this from falling = landed (grounded)

-- State ------------------------------------------------------------------------
local transitioning = false  -- gate input while the engine is in scene-load orchestration
local health = 1.0
local health_bar_id = 0
local camera_id = 0
local jump_state = "grounded"   -- "grounded" | "rising" | "falling"
local coyote_timer = 0.0
local jump_buffer_timer = 0.0

function on_create()
    log.info("Platformer player created with speed=" .. speed)

    local saved = gamestate.get("health", 0)
    if saved > 0 then health = saved else health = 1.0; gamestate.set("health", health) end

    -- Resume from a paused snapshot (ESC) if the snapshot was for this scene.
    local cx = gamestate.get("continue_x", 0)
    local cy = gamestate.get("continue_y", 0)
    if cx ~= 0 or cy ~= 0 then
        local cz = gamestate.get("continue_z", 0)
        transform.set_position(entity_id, cx, cy, cz)
        log.info("Resumed platformer at " .. cx .. ", " .. cy)
        gamestate.remove("continue_x")
        gamestate.remove("continue_y")
        gamestate.remove("continue_z")
        gamestate.remove("continue_scene")
    end

    health_bar_id = scene.find_entity("HealthBar")
    camera_id = scene.find_entity("MainCamera")
    if health_bar_id ~= 0 then ui.set_progress(health_bar_id, health) end
    ui.transition_fade_in(0.3)
end

local function follow_camera()
    if camera_id == 0 then return end
    local px, py, _ = transform.get_position(entity_id)
    local _, _, cz = transform.get_position(camera_id)
    transform.set_position(camera_id, px, py, cz)
end

local function update_jump_state(dt, vy)
    if jump_state == "grounded" then
        if vy > k_rise_threshold then
            jump_state = "rising"
        elseif vy < -k_fall_threshold then
            -- walked off a ledge (gravity pulled us down)
            jump_state = "falling"
            coyote_timer = k_coyote_time
        end
    elseif jump_state == "rising" then
        if vy < k_apex_threshold then
            jump_state = "falling"
            -- no coyote: this came from a deliberate jump, not a ledge slip
        end
    else -- falling
        coyote_timer = math.max(0, coyote_timer - dt)
        if vy > -k_land_threshold then
            jump_state = "grounded"
            coyote_timer = 0
        end
    end
end

function on_update(dt)
    -- Engine drives scene-to-scene handoffs through `scene.transition_to`. We
    -- still gate input here so the player can't move during the out-anim /
    -- loading screen.
    if transitioning or ui.is_transition_active() then
        return
    end

    local vx, vy = physics.get_velocity(entity_id)
    update_jump_state(dt, vy)

    local can_jump_now = (jump_state == "grounded") or (coyote_timer > 0)

    -- Horizontal input — A/D + arrow keys, or gamepad 0: left stick (dead zone applied) and D-pad.
    local moveX = 0
    if input.is_key_pressed(65) or input.is_key_pressed(263) then moveX = moveX - 1 end
    if input.is_key_pressed(68) or input.is_key_pressed(262) then moveX = moveX + 1 end
    if input.is_gamepad_connected(0) then
        local stickX, _ = input.get_gamepad_stick(0, 0)
        if input.is_gamepad_button_pressed(0, 14) then stickX = stickX - 1 end
        if input.is_gamepad_button_pressed(0, 12) then stickX = stickX + 1 end
        moveX = math.max(-1, math.min(1, moveX + stickX))
    end

    -- Crisp ground motion: directly set vx (no slide). Air motion is preserved
    -- (locked to whatever velocity the player jumped with).
    if jump_state == "grounded" then
        physics.set_velocity(entity_id, moveX * speed, vy)
    end

    -- Jump buffer: every fresh press extends the buffer; consumed on landing.
    if input.is_key_just_pressed(87) or input.is_key_just_pressed(32) or input.is_key_just_pressed(265)
            or input.is_gamepad_button_just_pressed(0, 0) then
        jump_buffer_timer = k_jump_buffer
    end
    jump_buffer_timer = math.max(0, jump_buffer_timer - dt)

    if jump_buffer_timer > 0 and can_jump_now then
        local nx, _ = physics.get_velocity(entity_id)
        physics.set_velocity(entity_id, nx, k_jump_speed)
        jump_buffer_timer = 0
        coyote_timer = 0
        jump_state = "rising"
    end

    -- HUD sync.
    health = gamestate.get("health", health)
    if health_bar_id ~= 0 then ui.set_progress(health_bar_id, health) end

    follow_camera()

    if health <= 0 then
        log.info("Health depleted in platformer level.")
        transitioning = true
        scene.transition_to("scenes/game_over.owl", "fade_out", 0.3)
        return
    end

    -- ESC or gamepad Start: pause + return to menu, with a continue snapshot.
    if input.is_key_just_pressed(256) or input.is_gamepad_button_just_pressed(0, 7) then
        local px, py, pz = transform.get_position(entity_id)
        gamestate.set("continue_scene", "scenes/platformer_house.owl")
        gamestate.set("continue_x", px)
        gamestate.set("continue_y", py)
        gamestate.set("continue_z", pz)
        gamestate.set("has_continue", true)
        log.info("ESC: pausing platformer — saved position for Continue.")
        transitioning = true
        scene.transition_to("scenes/main_menu.owl", "fade_out", 0.3)
    end
end

-- Box2D contact: fires once when the player starts touching another body
-- (ground tilemap, moving platform...), with that body's entity id.
function on_collision(other_id)
    log.trace("Player touched " .. entity.get_name(other_id) .. " (" .. jump_state .. ")")
end

function on_destroy()
    log.trace("Platformer player destroyed")
end
