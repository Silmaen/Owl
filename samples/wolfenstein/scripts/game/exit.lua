-- The exit elevator of a Wolfenstein mini floor: an invisible Trigger.Type=Interaction entity set in the switch wall,
-- bound with CallbackName=on_elevator_use (`interact` while the player stands next to the switch).
--
-- It closes the floor: the tally (score of the floor, items, secrets and map explored, in %) goes into `wolf_tally_*`,
-- a bonus of 10 points per percent is added to the score, the best ratios of the floor are kept in the settings
-- (`wolf_best_<floor>_items`, `_secrets`, `_explored`, saved to `settings.yml`), then the tally screen opens and leads to
-- the next floor of FLOORS, or to the victory after the last one.
-- The floors are listed here, not in a script property: the game export packs the scenes a script names.

local FLOORS = { "scenes/level_1.owl", "scenes/level_2.owl", "scenes/level_3.owl" }
local BONUS_PER_PERCENT = 10

local leaving = false

local function ratio(found, total)
    if total == 0 then
        return 100
    end
    return math.floor(100 * found / total)
end

local function keep_best(key, value)
    if value > settings.get(key, 0) then
        settings.set(key, value)
    end
end

function on_elevator_use()
    if leaving then
        return
    end
    leaving = true
    local level = gamestate.get("wolf_level", 1)
    local items = ratio(gamestate.get("wolf_items", 0), gamestate.get("wolf_items_total", 0))
    local secrets = ratio(gamestate.get("wolf_secrets", 0), gamestate.get("wolf_secrets_total", 0))
    local explored = gamestate.get("wolf_explored", 0)
    local bonus = (items + secrets + explored) * BONUS_PER_PERCENT
    local score = gamestate.get("wolf_score", 0) + bonus
    gamestate.set("wolf_score", score)
    gamestate.set("wolf_tally_level", level)
    gamestate.set("wolf_tally_items", items)
    gamestate.set("wolf_tally_secrets", secrets)
    gamestate.set("wolf_tally_explored", explored)
    gamestate.set("wolf_tally_bonus", bonus)
    gamestate.set("wolf_tally_floor_score", score - gamestate.get("wolf_floor_score", 0))
    gamestate.set("wolf_next_scene", FLOORS[level + 1] or "scenes/victory.owl")
    gamestate.set("wolf_leaving", 1)
    local best = "wolf_best_" .. level .. "_"
    keep_best(best .. "items", items)
    keep_best(best .. "secrets", secrets)
    keep_best(best .. "explored", explored)
    settings.save()
    sound.play("sounds/game/elevator.wav")
    scene.transition_to("scenes/tally.owl", "fade", 0.6)
end
