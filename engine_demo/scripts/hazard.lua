-- Hazard of the platformer: a spike trap or a pit. Bound to a trigger (`Type: Target`, a passive box) whose enter edge
-- calls `on_trigger_enter`. The hazard only leaves a request in the gamestate; platformer_player.lua applies it (health,
-- knock-back, invulnerability, camera shake, fade and respawn at the last checkpoint).
-- `kind` is "spike" (a third of the health bar and a kick away from the trap) or "pit" (a fall: costs health, respawns).

properties = {
    { name = "kind",   type = "string", default = "spike" },
    { name = "damage", type = "float",  default = 0.34 },
}

function on_trigger_enter(player_id)
    if kind == "pit" then
        gamestate.set("platformer_fall", 1)
        log.info("Fell into a pit")
        return
    end
    local x, _, _ = transform.get_position(entity_id)
    gamestate.set("platformer_hit", damage)
    gamestate.set("platformer_hit_x", x)
    log.info("Spike hit!")
end
