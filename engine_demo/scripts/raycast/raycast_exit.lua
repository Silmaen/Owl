-- Elevator of the raycast level: the exit of E1M1.
-- Demonstrates: Trigger.Type=Interaction on an invisible entity set in a wall, a string property, gamestate as the
--               victory flag (`level_raycast_done`), scene.transition_to back to the hub.
--
-- `kind` is "main" for the exit elevator, "secret" for the one hidden behind the pushwalls (the secret floor of
-- Wolfenstein 3D: here it ends the level too, with a bonus line in the log).

properties = {
    { name = "kind", type = "string", default = "main" },
}

local leaving = false

-- Bound by Trigger.Type=Interaction with CallbackName=on_elevator_use.
function on_elevator_use(player_id)
    if leaving then
        return
    end
    leaving = true
    gamestate.set("level_raycast_done", 1)
    gamestate.set("raycast_secret_exit", kind == "secret" and 1 or 0)
    -- The next visit starts at the entrance again.
    gamestate.remove("raycast_return_x")
    gamestate.remove("raycast_return_y")
    gamestate.remove("raycast_return_z")
    log.info("Elevator taken (" .. kind .. "): E1M1 complete")
    sound.play("sounds/raycast/elevator.wav")
    scene.transition_to("scenes/world_map.owl", "wipe_right", 0.4)
end
