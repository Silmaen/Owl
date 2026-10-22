-- Heart pick-up: restores a third of the health bar. Bound by Trigger.Type=LuaCallback with CallbackName=on_heart_touched
-- (fires every frame while the player overlaps): a full bar leaves the heart where it is for later. Same pop as the
-- coins, with a gentle bob while waiting.

properties = {
    { name = "heal", type = "float", default = 0.34 },
    { name = "pop_time", type = "float", default = 0.3 },
}

local taken = false
local clock = 0.0
local bob = 0.0
local home_x, home_y, home_z = 0.0, 0.0, 0.0
local base_scale = 1.0

function on_create()
    base_scale = transform.get_scale(entity_id)
end

function on_heart_touched(player_id)
    if taken then return end
    local health = gamestate.get("health", 1.0)
    if health >= 1.0 then return end
    taken = true
    home_x, home_y, home_z = transform.get_position(entity_id)
    home_y = home_y - bob
    gamestate.set("health", math.min(1.0, health + heal))
    gamestate.set("platformer_banner", "Health restored")
    sound.play("sounds/platformer/coin_collect.wav")
    clock = 0.0
end

function on_update(dt)
    clock = clock + dt
    if not taken then
        -- Bob around wherever the heart is (its position is only nudged, never restored).
        local x, y, z = transform.get_position(entity_id)
        local next_bob = 0.08 * math.sin(clock * 3.0)
        transform.set_position(entity_id, x, y - bob + next_bob, z)
        bob = next_bob
        return
    end
    local t = math.min(1.0, clock / pop_time)
    local s = base_scale * (1.0 + 1.1 * t)
    transform.set_scale(entity_id, s, s, 1)
    transform.set_position(entity_id, home_x, home_y + 0.8 * t, home_z)
    entity.set_tint(entity_id, 1.0, 1.0, 1.0, 1.0 - t)
    if t >= 1.0 then scene.destroy_entity(entity_id) end
end
