-- Victory screen of Sokoban, after the Engine Demo (scripts/ui/victory.lua): reached when the last level is done; it
-- shows how many levels are done and the sum of their best moves (the `sokoban_*` settings), over the levels of
-- `tables/levels.owltable` (scripts/game/levels.lua).

function on_create()
    local levels = require("scripts.game.levels")
    local count = levels.count()
    if not ui.is_transition_active() then ui.transition_play("wipe_left", 0.4, 0.2, 0.12, 0.05) end
    sound.play("sounds/ui/victory.wav")
    local done, total = 0, 0
    for i = 1, count do
        if levels.done(i) then
            done = done + 1
            total = total + levels.best(i)
        end
    end
    local text = "Levels done: " .. done .. " / " .. count .. "\nBest moves, all levels: " .. total
    if done < count then
        text = text .. "\nThe level select shows the ones left."
    end
    local label = scene.find_entity("ScoreLabel")
    if label ~= 0 then
        ui.set_text(label, text)
    end
end

function on_levels_clicked()
    scene.transition_to("scenes/level_select.owl", "wipe_left", 0.3, 0.2, 0.12, 0.05)
end

function on_title_clicked()
    scene.transition_to("scenes/title.owl", "wipe_left", 0.3, 0.2, 0.12, 0.05)
end
