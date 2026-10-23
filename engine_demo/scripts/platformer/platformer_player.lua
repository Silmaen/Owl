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
--
-- Feel of the level:
--   * the camera leads in the running direction, follows with a smoothing, snaps to whole screen pixels and shakes
--     on damage;
--   * hazards (hazard.lua) leave a request in the gamestate that is read here: a hit costs health, flashes the
--     sprite, shakes the camera, kicks the player back and gives a short invulnerability; a fall into a pit costs
--     health and respawns at the last checkpoint behind a fade (checkpoint.lua stores it in the gamestate);
--   * `platformer_banner` (a gamestate string) replaces the objective text for a moment.

properties = {
    { name = "speed", type = "float", default = 5.5 },
    { name = "bounds_min_x", type = "float", default = 12.0 },   -- limits of the camera centre (world units)
    { name = "bounds_max_x", type = "float", default = 185.0 },
    { name = "bounds_min_y", type = "float", default = -13.2 },
    { name = "bounds_max_y", type = "float", default = 5.3 },
    { name = "jump_speed", type = "float", default = 7.5 },   -- absolute upward velocity when jump fires
    { name = "coyote_time", type = "float", default = 0.18 },   -- s of grace after walking off a ledge
    { name = "jump_buffer", type = "float", default = 0.15 },   -- s before landing that a jump press is remembered
    { name = "rise_threshold", type = "float", default = 3.0 },   -- vy above this = "rising" (jumped or rocketed up)
    { name = "fall_threshold", type = "float", default = 3.0 },   -- vy below -threshold = "falling" (or walked off)
    { name = "camera_lift", type = "float", default = 3.0 },   -- camera height above the player (world units)
    { name = "apex_threshold", type = "float", default = 0.5 },   -- vy below this from rising = transition to falling
    { name = "land_threshold", type = "float", default = 0.5 },   -- vy above -this from falling = landed (grounded)
    { name = "lead_distance", type = "float", default = 2.6 },   -- camera look-ahead in the running direction (world units)
    { name = "lead_follow", type = "float", default = 2.5 },   -- 1/s: how fast the look-ahead settles
    { name = "cam_follow_x", type = "float", default = 7.0 },   -- 1/s: horizontal camera smoothing
    { name = "cam_follow_y", type = "float", default = 4.0 },   -- 1/s: vertical camera smoothing
    { name = "pixels_per_unit", type = "float", default = 54.0 },   -- 3x the 18 px tiles at 720p: the camera moves by whole screen pixels
    { name = "invulnerable", type = "float", default = 1.2 },   -- s of invulnerability after a hit
    { name = "knock_x", type = "float", default = 4.5 },   -- knock-back speed away from the hazard
    { name = "knock_y", type = "float", default = 6.0 },
    { name = "shake_time", type = "float", default = 0.35 },   -- s of camera shake after a hit
    { name = "shake_amount", type = "float", default = 0.28 },   -- world units at the start of the shake
    { name = "pit_cost", type = "float", default = 0.34 },   -- health lost by a fall into a pit (a third of the bar, like a spike)
    { name = "banner_time", type = "float", default = 2.4 },   -- s a banner replaces the objective text
    { name = "frame_idle", type = "int", default = 150, min = 0 },
    { name = "frame_step", type = "int", default = 151, min = 0 },  -- second pose: walk cycle (alternating with idle) and jump
    { name = "step_time", type = "float", default = 0.12, min = 0.01 },  -- s per walk frame
}

-- Tunables ---------------------------------------------------------------------
local k_jump_speed, k_coyote_time, k_jump_buffer, k_rise_threshold, k_fall_threshold, k_camera_lift, k_apex_threshold, k_land_threshold, k_lead_distance, k_lead_follow, k_cam_follow_x, k_cam_follow_y, k_pixels_per_unit, k_invulnerable, k_knock_x, k_knock_y, k_shake_time, k_shake_amount, k_pit_cost, k_banner_time    -- set from the properties in on_create

-- State ------------------------------------------------------------------------
local transitioning = false  -- gate input while the engine is in scene-load orchestration
local health = 1.0
local health_bar_id = 0
local tool_cursor_id = 0
local tool_active = false
local camera_id = 0
local jump_state = "grounded"   -- "grounded" | "rising" | "falling"
local coyote_timer = 0.0
local jump_buffer_timer = 0.0
local cam_x, cam_y, cam_z = 0.0, 0.0, 5.0
local lead = 0.0
local shake_left = 0.0
local invulnerable_left = 0.0
local respawn_state = nil        -- nil | "out" (fading to black, then teleport) | "in"
local objective_id = 0
local banner_left = 0.0
local zone_name = ""
local dying = false

-- Sprite: frames of the Pixel Platformer characters (24 px grid of platformer_atlas.png).
local sprite_id = 0
local sprite_scale = 1.0
local walk_clock = 0.0

-- The three zones of the level, by the x of their first tile.
local k_zones = {
    { x = 0,   name = "Meadow" },
    { x = 56,  name = "Farm" },
    { x = 133, name = "Factory" },
}
local k_objective = "reach the door at the top of the factory tower"

local function snap(value)
    return math.floor(value * k_pixels_per_unit + 0.5) / k_pixels_per_unit
end

local function clamp(value, low, high)
    return math.max(low, math.min(high, value))
end

-- Objective text of the HUD: the zone the player is in, unless a banner is showing.
local function show_zone(x, force)
    local name = k_zones[1].name
    for _, zone in ipairs(k_zones) do
        if x >= zone.x then name = zone.name end
    end
    if name ~= zone_name or force then
        zone_name = name
        if objective_id ~= 0 and (banner_left <= 0 or force) then
            ui.set_text(objective_id, name .. " - Objective: " .. k_objective)
        end
    end
end

local function show_banner(text)
    if objective_id == 0 then return end
    ui.set_text(objective_id, text)
    banner_left = k_banner_time
end

-- Camera ------------------------------------------------------------------------
local function place_camera(dt)
    if camera_id == 0 then return end
    local ox, oy = 0.0, 0.0
    if shake_left > 0 then
        shake_left = math.max(0, shake_left - dt)
        local amount = k_shake_amount * (shake_left / k_shake_time)
        ox = (math.random() * 2 - 1) * amount
        oy = (math.random() * 2 - 1) * amount
    end
    transform.set_position(camera_id, snap(cam_x + ox), snap(cam_y + oy), cam_z)
end

local function follow_camera(dt, vx, snap_now)
    if camera_id == 0 then return end
    local px, py, _ = transform.get_position(entity_id)
    local want_lead = 0.0
    if vx > 0.5 then want_lead = k_lead_distance elseif vx < -0.5 then want_lead = -k_lead_distance end
    lead = lead + (want_lead - lead) * (1 - math.exp(-k_lead_follow * dt))
    local target_x = clamp(px + lead, bounds_min_x, bounds_max_x)
    local target_y = clamp(py + k_camera_lift, bounds_min_y, bounds_max_y)   -- the floor sits in the lower third
    if snap_now then
        cam_x, cam_y = target_x, target_y
    else
        cam_x = cam_x + (target_x - cam_x) * (1 - math.exp(-k_cam_follow_x * dt))
        cam_y = cam_y + (target_y - cam_y) * (1 - math.exp(-k_cam_follow_y * dt))
    end
    place_camera(dt)
end

-- Damage ------------------------------------------------------------------------
local function set_health(value)
    health = clamp(value, 0.0, 1.0)
    gamestate.set("health", health)
    if health_bar_id ~= 0 then ui.set_progress(health_bar_id, health) end
end

local function flash(on)
    if sprite_id == 0 then return end
    if on then
        entity.set_tint(sprite_id, 1.0, 0.35, 0.35, 0.65)
    else
        entity.set_tint(sprite_id, 1.0, 1.0, 1.0, 1.0)
    end
end

local function die()
    if dying then return end
    dying = true
    flash(true)
    shake_left = k_shake_time
    log.info("Health depleted in platformer level.")
    transitioning = true
    scene.transition_to("scenes/game_over.owl", "fade_out", 0.5)
end

local function take_hit(amount, from_x)
    if invulnerable_left > 0 then return end
    set_health(health - amount)
    sound.play("sounds/platformer/jump.wav")
    if health <= 0 then die() return end
    invulnerable_left = k_invulnerable
    shake_left = k_shake_time
    local px, _, _ = transform.get_position(entity_id)
    local away = (px >= from_x) and 1 or -1
    physics.set_velocity(entity_id, away * k_knock_x, k_knock_y)
    jump_state = "rising"
    coyote_timer = 0
end

local function start_fall()
    if respawn_state ~= nil or dying then return end
    set_health(health - k_pit_cost)
    shake_left = k_shake_time
    if health <= 0 then die() return end
    respawn_state = "out"
    physics.set_velocity(entity_id, 0, 0)
    physics.set_gravity_scale(entity_id, 0)
    ui.transition_fade_out(0.25)
end

local function finish_respawn()
    local cx = gamestate.get("platformer_checkpoint_x", 4.5)
    local cy = gamestate.get("platformer_checkpoint_y", -14.4)
    physics.set_transform(entity_id, cx, cy, 0)
    transform.set_position(entity_id, cx, cy, 0.5)
    physics.set_velocity(entity_id, 0, 0)
    physics.set_gravity_scale(entity_id, 1)
    jump_state = "falling"
    coyote_timer = 0
    jump_buffer_timer = 0
    lead = 0.0
    invulnerable_left = k_invulnerable
    follow_camera(0.016, 0, true)
    ui.transition_fade_in(0.3)
    respawn_state = "in"
end

-- Requests left in the gamestate by the other scripts of the level.
local function read_requests()
    local damage = gamestate.get("platformer_hit", 0)
    if damage > 0 then
        local from_x = gamestate.get("platformer_hit_x", 0)
        gamestate.remove("platformer_hit")
        gamestate.remove("platformer_hit_x")
        take_hit(damage, from_x)
    end
    if gamestate.get("platformer_fall", 0) > 0 then
        gamestate.remove("platformer_fall")
        start_fall()
    end
    local banner = gamestate.get("platformer_banner", "")
    if banner ~= "" then
        gamestate.remove("platformer_banner")
        show_banner(banner)
    end
end

function on_create()
    k_jump_speed = jump_speed
    k_coyote_time = coyote_time
    k_jump_buffer = jump_buffer
    k_rise_threshold = rise_threshold
    k_fall_threshold = fall_threshold
    k_camera_lift = camera_lift
    k_apex_threshold = apex_threshold
    k_land_threshold = land_threshold
    k_lead_distance = lead_distance
    k_lead_follow = lead_follow
    k_cam_follow_x = cam_follow_x
    k_cam_follow_y = cam_follow_y
    k_pixels_per_unit = pixels_per_unit
    k_invulnerable = invulnerable
    k_knock_x = knock_x
    k_knock_y = knock_y
    k_shake_time = shake_time
    k_shake_amount = shake_amount
    k_pit_cost = pit_cost
    k_banner_time = banner_time
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
    else
        -- A fresh entry from the hub starts from the beginning of the level.
        gamestate.set("platformer_checkpoint_x", 4.5)
        gamestate.set("platformer_checkpoint_y", -14.4)
    end
    gamestate.remove("platformer_hit")
    gamestate.remove("platformer_fall")
    gamestate.remove("platformer_banner")

    health_bar_id = scene.find_entity("HealthBar")
    camera_id = scene.find_entity("MainCamera")
    sprite_id = scene.find_entity("PlayerSprite")
    objective_id = ui.find("ObjectiveText")
    if sprite_id ~= 0 then sprite_scale = transform.get_scale(sprite_id) end
    tool_cursor_id = ui.find("BuildCursor")
    if health_bar_id ~= 0 then ui.set_progress(health_bar_id, health) end
    if camera_id ~= 0 then
        local _, _, z = transform.get_position(camera_id)
        cam_z = z
    end
    follow_camera(0.016, 0, true)
    local px, _, _ = transform.get_position(entity_id)
    show_zone(px)
    ui.transition_fade_in(0.3)
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

-- Idle / walk / jump frame and facing of the child sprite.
local function animate(dt, vx)
    if sprite_id == 0 then return end
    local frame = frame_idle
    if jump_state ~= "grounded" then
        frame = frame_step
    elseif math.abs(vx) > 0.5 then
        walk_clock = walk_clock + dt
        frame = (math.floor(walk_clock / step_time) % 2 == 0) and frame_idle or frame_step
    else
        walk_clock = 0.0
    end
    isometric.set_sprite_frame(sprite_id, frame)
    if vx > 0.5 then
        transform.set_scale(sprite_id, sprite_scale, sprite_scale, 1)
    elseif vx < -0.5 then
        transform.set_scale(sprite_id, -sprite_scale, sprite_scale, 1)
    end
end

-- Invulnerability blink, banner timeout and the fade of a respawn.
local function update_effects(dt)
    if invulnerable_left > 0 then
        invulnerable_left = math.max(0, invulnerable_left - dt)
        if invulnerable_left > 0 then
            flash(math.floor(invulnerable_left * 12) % 2 == 0)
        elseif not dying then
            flash(false)
        end
    end
    if banner_left > 0 then
        banner_left = banner_left - dt
        if banner_left <= 0 then
            local px, _, _ = transform.get_position(entity_id)
            show_zone(px, true)
        end
    end
    if respawn_state == "out" and not ui.is_transition_active() then
        finish_respawn()
    elseif respawn_state == "in" and not ui.is_transition_active() then
        respawn_state = nil
    end
end

function on_update(dt)
    -- Engine drives scene-to-scene handoffs through `scene.transition_to`. We
    -- still gate input here so the player can't move during the out-anim /
    -- loading screen.
    if transitioning then
        return
    end

    local vx, vy = physics.get_velocity(entity_id)
    read_requests()
    update_effects(dt)
    if transitioning then return end
    if respawn_state == "out" then
        follow_camera(dt, 0, false)
        return
    end

    update_jump_state(dt, vy)

    local can_jump_now = (jump_state == "grounded") or (coyote_timer > 0)
    animate(dt, vx)

    -- Horizontal input: the `platformer_move` axis of input_actions.yml (A/D, arrows, left stick, D-pad).
    local moveX = input.get_action_value("platformer_move")

    -- Crisp ground motion: directly set vx (no slide). Air motion is preserved
    -- (locked to whatever velocity the player jumped with).
    if jump_state == "grounded" then
        physics.set_velocity(entity_id, moveX * speed, vy)
    end

    -- Jump buffer: every fresh press extends the buffer; consumed on landing.
    if input.is_action_just_pressed("platformer_jump") then
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

    -- HUD sync (a heart picked up elsewhere changes the gamestate).
    health = gamestate.get("health", health)
    if health_bar_id ~= 0 then ui.set_progress(health_bar_id, health) end

    follow_camera(dt, vx, false)
    local px, _, _ = transform.get_position(entity_id)
    show_zone(px)

    -- T: the tool cursor (a HUD sprite following the mouse) as a construction tool would show it.
    if tool_cursor_id ~= 0 and input.is_action_just_pressed("toggle_tool") then
        tool_active = not tool_active
        ui.set_tool_cursor(tool_cursor_id, tool_active)
    end

    if health <= 0 then
        die()
        return
    end

    -- ESC or gamepad Start: pause + return to menu, with a continue snapshot.
    if input.is_action_just_pressed("pause") then
        local py, pz
        px, py, pz = transform.get_position(entity_id)
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
