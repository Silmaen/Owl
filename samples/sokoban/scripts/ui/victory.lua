-- Victory screen of Sokoban, after the Engine Demo (scripts/ui/victory.lua): reached when the last level is done; it
-- shows how many levels are done and the sum of their best moves (the `sokoban_*` settings).

local LEVEL_COUNT = 10

function on_create()
    if not ui.is_transition_active() then ui.transition_play("wipe_left", 0.4, 0.2, 0.12, 0.05) end
    sound.play("sounds/ui/victory.wav")
    local done, total = 0, 0
    for i = 1, LEVEL_COUNT do
        if settings.get(string.format("sokoban_done_%02d", i), false) then
            done = done + 1
            total = total + settings.get(string.format("sokoban_best_%02d", i), 0)
        end
    end
    local text = "Levels done: " .. done .. " / " .. LEVEL_COUNT .. "\nBest moves, all levels: " .. total
    if done < LEVEL_COUNT then
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
