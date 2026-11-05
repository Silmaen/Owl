-- A chalk line on the floor in front of the barrel stack: a sensor body that stops nothing and counts the barrels
-- rolling over it in the game state (`barrels_over_line`).
-- Demonstrates: a 3D sensor PhysicBody and on_trigger_enter / on_trigger_exit from the physics.

function on_trigger_enter(other)
    gamestate.set("barrels_over_line", gamestate.get("barrels_over_line", 0) + 1)
end

function on_trigger_exit(other)
    gamestate.set("barrels_past_line", gamestate.get("barrels_past_line", 0) + 1)
end
