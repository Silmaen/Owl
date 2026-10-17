-- Spike trap — deducts a chunk of health on each entry edge.
-- Bound to Trigger.Type=LuaCallback with CallbackName=on_spike_hit.

local damage = 0.34  -- ~3 hits to kill

function on_spike_hit(player_id)
    local h = gamestate.get("health", 1.0)
    h = h - damage
    if h < 0 then h = 0 end
    gamestate.set("health", h)
    log.info(string.format("Spike hit! Health -> %.2f", h))
    sound.play("sounds/jump.wav")  -- reuse a short sound for feedback
end
