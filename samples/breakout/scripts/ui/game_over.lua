-- Game over screen of Breakout: the score counted up arcade style, the best one (the `breakout_best` setting, kept
-- when beaten), Retry the level lost with three new lives, or back to the title.

local WIPE = { 0.45, 0.05, 0.55 }
local COUNT_TIME = 1.0

local score, shown, clock = 0, -1, 0.0
local score_id = 0
local leaving = false

function on_create()
    if not ui.is_transition_active() then
        ui.transition_play("wipe_up", 0.4, WIPE[1], WIPE[2], WIPE[3])
    end
    score = gamestate.get("breakout_score", 0)
    local best = settings.get("breakout_best", 0)
    local best_text = string.format("HI-SCORE %06d", best)
    if score > best then
        settings.set("breakout_best", score)
        settings.save()
        best_text = "NEW HI-SCORE!"
    end
    score_id = scene.find_entity("ScoreLabel")
    local best_id = scene.find_entity("BestLabel")
    if best_id ~= 0 then ui.set_text(best_id, best_text) end
end

function on_update(dt)
    clock = math.min(COUNT_TIME, clock + dt)
    local value = math.floor(score * clock / COUNT_TIME + 0.5)
    if value ~= shown and score_id ~= 0 then
        shown = value
        ui.set_text(score_id, string.format("SCORE %06d", value))
    end
end

function on_retry_clicked()
    if leaving then return end
    leaving = true
    local level = gamestate.get("breakout_level", 1)
    local target = gamestate.get("breakout_scene", "scenes/level_1.owl")
    gamestate.clear()
    gamestate.set("breakout_score", 0)
    gamestate.set("breakout_lives", 3)
    gamestate.set("breakout_level", level)
    gamestate.set("breakout_scene", target)
    scene.transition_to(target, "wipe_up", 0.3, WIPE[1], WIPE[2], WIPE[3])
end

function on_menu_clicked()
    if leaving then return end
    leaving = true
    gamestate.clear()
    scene.transition_to("scenes/title.owl", "wipe_up", 0.3, WIPE[1], WIPE[2], WIPE[3])
end
