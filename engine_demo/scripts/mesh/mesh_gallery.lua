-- Mesh gallery: Escape (or Start) goes back to the main menu; the FlyCamera component moves the view.
-- Demonstrates: MeshRenderer entities drawn by the mesh_world layer, input actions, scene.transition_to.

local leaving = false

function on_update(dt)
    if not leaving and input.is_action_pressed("pause") then
        leaving = true
        scene.transition_to("scenes/main_menu.owl", "fade", 0.3)
    end
end
