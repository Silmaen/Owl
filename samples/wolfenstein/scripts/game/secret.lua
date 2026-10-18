-- A secret wall of Wolfenstein mini (`RaycastPushWall`, tag `Secret`): `interact` next to it slides it (built in).
-- The first time it leaves the `idle` state it counts as a found secret (`wolf_secrets`), once: the mark lives in the
-- gamestate (`wolf_secret_<entity id>`), so a loaded save does not count a moved wall again.

local key = ""
local counted = false

function on_create()
    key = "wolf_secret_" .. entity_id
    counted = gamestate.get(key, 0) == 1
end

function on_update(dt)
    if counted or pushwall.get_state(entity_id) == "idle" then
        return
    end
    counted = true
    gamestate.set(key, 1)
    gamestate.set("wolf_secrets", gamestate.get("wolf_secrets", 0) + 1)
    gamestate.set("wolf_toast", "Secret found!")
    sound.play("sounds/game/pushwall.wav")
end
