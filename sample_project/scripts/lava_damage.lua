-- Lava patch — bleeds health while the player overlaps the trigger volume.
-- Bound to Trigger.Type=LuaCallback with CallbackName=on_lava_tick.
-- LuaCallback fires every frame the player is inside, which is the rate we
-- want for a continuous "burning" effect.

local damage_per_second = 0.6  -- ~1.6 seconds to die from full health

function on_lava_tick(other_id)
    local dt = time.delta()
    if dt <= 0 then dt = 1.0 / 60.0 end
    local h = gamestate.get("health", 1.0)
    h = h - damage_per_second * dt
    if h < 0 then h = 0 end
    gamestate.set("health", h)
end

