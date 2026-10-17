-- Checkpoint of the platformer: a trigger (`Type: Target`) over a flag post. The first time the player enters it, the
-- respawn point moves here (`platformer_checkpoint_x` / `_y` in the gamestate, read by platformer_player.lua and kept
-- in the saves), the flag is raised and waves (frames of platformer_ext_atlas.png), and a banner is shown.
-- `flag` is the tag of the flag sprite entity, which this script drives with `isometric.set_sprite_frame`.

properties = {
    { name = "flag", type = "string", default = "" },
}

local k_frame_idle = 240
local k_frame_wave = { 241, 242, 243, 242 }
local k_wave_time = 0.16
local k_pop_time = 0.4

local reached = false
local flag_id = 0
local flag_scale = 1.0
local clock = 0.0
local pop = 0.0

local function raise()
    reached = true
    if flag_id ~= 0 then isometric.set_sprite_frame(flag_id, k_frame_wave[1]) end
end

function on_create()
    flag_id = scene.find_entity(flag)
    if flag_id ~= 0 then
        flag_scale = transform.get_scale(flag_id)
        isometric.set_sprite_frame(flag_id, k_frame_idle)
    end
    -- A run continued from a pause or a save: the flags up to the stored checkpoint stay raised.
    local x, _, _ = transform.get_position(entity_id)
    if gamestate.get("platformer_checkpoint_x", 0) >= x - 0.01 and gamestate.get("platformer_checkpoint_x", 0) > 5 then
        raise()
    end
end

function on_trigger_enter(player_id)
    if reached then return end
    local x, y, _ = transform.get_position(entity_id)
    gamestate.set("platformer_checkpoint_x", x)
    gamestate.set("platformer_checkpoint_y", y + 0.1)
    gamestate.set("platformer_banner", "Checkpoint reached")
    sound.play("sounds/platformer/coin_collect.wav")
    log.info("Checkpoint at " .. x .. ", " .. y)
    raise()
    pop = k_pop_time
end

function on_update(dt)
    if not reached or flag_id == 0 then return end
    clock = clock + dt
    local frame = math.floor(clock / k_wave_time) % #k_frame_wave + 1
    isometric.set_sprite_frame(flag_id, k_frame_wave[frame])
    if pop > 0 then
        pop = math.max(0, pop - dt)
        local s = flag_scale * (1.0 + 0.35 * math.sin((1.0 - pop / k_pop_time) * math.pi))
        transform.set_scale(flag_id, s, s, 1)
    end
end
