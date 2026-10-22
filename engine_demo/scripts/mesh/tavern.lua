-- Tavern scene script: Escape (or Start) returns to the main menu. The fire, the candles and their flame cards flicker
-- through `LightFlicker` components; the FlyCamera component moves the view.
-- Demonstrates: input.is_action_pressed, scene.transition_to.

local leaving = false

function on_update(_)
    if not leaving and input.is_action_pressed("pause") then
        leaving = true
        scene.transition_to("scenes/main_menu.owl", "fade", 0.3)
    end
end
