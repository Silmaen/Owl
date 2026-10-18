-- Tally screen of Wolfenstein mini, between two floors: what exit.lua wrote into `wolf_tally_*`, the best ratios of the
-- floor (settings `wolf_best_<floor>_*`), and a button to the next floor (`wolf_next_scene`, the victory after the last).

local leaving = false

local function set(name, text)
    local id = scene.find_entity(name)
    if id ~= 0 then
        ui.set_text(id, text)
    end
end

function on_create()
    ui.transition_fade_in(0.5)
    local level = gamestate.get("wolf_tally_level", 1)
    local best = "wolf_best_" .. level .. "_"
    set("TallyTitle", "Floor " .. level .. " completed")
    set("ItemsLabel", string.format("Items found      %3d%%", gamestate.get("wolf_tally_items", 0)))
    set("SecretsLabel", string.format("Secrets found    %3d%%", gamestate.get("wolf_tally_secrets", 0)))
    set("MapLabel", string.format("Map explored     %3d%%", gamestate.get("wolf_tally_explored", 0)))
    set("ScoreLabel", string.format("Floor score %d  (bonus %d)    Total %d", gamestate.get("wolf_tally_floor_score", 0),
            gamestate.get("wolf_tally_bonus", 0), gamestate.get("wolf_score", 0)))
    set("BestLabel", string.format("Best on this floor: items %d%%, secrets %d%%, map %d%%", settings.get(best .. "items", 0),
            settings.get(best .. "secrets", 0), settings.get(best .. "explored", 0)))
    if gamestate.get("wolf_next_scene", "") == "scenes/victory.owl" then
        set("NextButton", "Finish")
    end
end

function on_next_clicked()
    if leaving then
        return
    end
    leaving = true
    sound.play("sounds/ui/start.wav")
    scene.transition_to(gamestate.get("wolf_next_scene", "scenes/title.owl"), "fade", 0.4)
end
