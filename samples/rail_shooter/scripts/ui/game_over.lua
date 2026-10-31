-- Game over screen of Ring Runner: the score counted up, how much of the lane was flown, the best score (the
-- `rail_best` setting, kept when beaten), Retry the level or back to the title.

properties = {
    { name = "count_time", type = "float", default = 1.0, min = 0.01 },
    { name = "sweep_color", type = "color", default = { 0.0, 0.05, 0.14, 1 } },
}

local score, shown, clock = 0, -1, 0.0
local score_id = 0
local leaving = false

function on_create()
    if not ui.is_transition_active() then ui.transition_play("fade", 0.8, 0.25, 0.02, 0.04) end
    sound.play("sounds/ui/game_over.wav")
    score = gamestate.get("rail_result_score", 0)
    local best = settings.get("rail_best", 0)
    local best_text = string.format("BEST %06d", best)
    if score > best then
        settings.set("rail_best", score)
        settings.save()
        best_text = "NEW BEST SCORE!"
    end
    score_id = scene.find_entity("ScoreLabel")
    local best_id = scene.find_entity("BestLabel")
    if best_id ~= 0 then ui.set_text(best_id, best_text) end
    local distance_id = scene.find_entity("DistanceLabel")
    if distance_id ~= 0 then
        ui.set_text(distance_id, string.format("LANE FLOWN %d%%", gamestate.get("rail_result_distance", 0)))
    end
end

function on_update(dt)
    clock = math.min(count_time, clock + dt)
    local value = math.floor(score * clock / count_time + 0.5)
    if value ~= shown and score_id ~= 0 then
        shown = value
        ui.set_text(score_id, string.format("SCORE %06d", value))
        gamestate.set("rail_shown_score", value)
    end
end

local function leave(path)
    if leaving then return end
    leaving = true
    gamestate.clear()
    scene.transition_to(path, "wipe_left", 0.45, sweep_color.r, sweep_color.g, sweep_color.b)
end

function on_retry_clicked()
    leave("scenes/level.owl")
end

function on_menu_clicked()
    leave("scenes/title.owl")
end
