-- Character yard scene script: a skinned mannequin whose `Animator` walks and idles in turn, waves on N, and holds a
-- lantern snapped on the `hand_r` socket of its mesh; M drops the lantern and picks it up again. Escape (or Start)
-- goes back to the menu.
-- Demonstrates: animator.set_float / trigger / get_state, socket.attach / detach / get_position, ui.set_text.

properties = {
    { name = "menu_scene", type = "string", default = "scenes/main_menu.owl" },
    { name = "character", type = "string", default = "Mannequin" },
    { name = "lantern", type = "string", default = "Lantern" },
    { name = "walk_seconds", type = "float", default = 4.0 },
    { name = "idle_seconds", type = "float", default = 3.0 },
    { name = "hand_socket", type = "string", default = "hand_r" },
}

local character_id = 0
local lantern_id = 0
local readout_id = 0
local leaving = false
local walking = false
local phase_time = 0.0
local held = true

function on_create()
    character_id = scene.find_entity(character)
    lantern_id = scene.find_entity(lantern)
    readout_id = ui.find("CharacterReadout")
    ui.transition_fade_in(0.4)
end

function on_update(dt)
    if leaving or ui.is_transition_active() then
        return
    end
    if input.is_action_just_pressed("pause") then
        leaving = true
        scene.transition_to(menu_scene, "fade_out", 0.3)
        return
    end
    if character_id == 0 then
        return
    end
    phase_time = phase_time + dt
    if phase_time >= (walking and walk_seconds or idle_seconds) then
        phase_time = 0.0
        walking = not walking
        animator.set_float(character_id, "speed", walking and 1.0 or 0.0)
    end
    if input.is_action_just_pressed("character_wave") then
        animator.trigger(character_id, "wave")
    end
    if lantern_id ~= 0 and input.is_action_just_pressed("character_drop") then
        if held then
            socket.detach(lantern_id)
        else
            socket.attach(lantern_id, character_id, hand_socket)
        end
        held = not held
    end
    if readout_id ~= 0 then
        local state = animator.get_state(character_id) or "-"
        local x, y, z = socket.get_position(character_id, hand_socket)
        local hand = x and string.format("%.2f, %.2f, %.2f", x, y, z) or "-"
        ui.set_text(readout_id, string.format("State: %s   Hand: %s   N: wave   M: %s the lantern", state, hand,
            held and "drop" or "take"))
    end
end
