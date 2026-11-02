-- Title of The Moon Lantern: a new game (the state cleared, Wren in the cottage), continue the save of slot 1, the
-- settings and quit. The hero waves now and then in the glade behind the menu.

properties = {
    { name = "first_room", type = "string", default = "scenes/cottage.owl" },
    { name = "wave_every", type = "float", default = 6.0, min = 1 },
}

local hero = 0
local timer = 0
local leaving = false

function on_create()
    hero = scene.find_entity("Hero")
    local continue = ui.find("ContinueButton")
    ui.set_button_enabled(continue, save.has_save(1))
    gamestate.set("title_has_save", save.has_save(1))
    if not ui.is_transition_active() then ui.transition_play("fade_in", 0.6, 0.02, 0.02, 0.05) end
end

function on_update(dt)
    timer = timer + dt
    if timer > wave_every then
        timer = 0
        animator.trigger(hero, "act")
    end
end

function on_new_clicked()
    if leaving then return end
    leaving = true
    sound.play("sounds/click.wav")
    gamestate.clear()
    gamestate.set("entry", "start")
    scene.transition_to(first_room, "fade_out", 0.6, 0.02, 0.02, 0.05)
end

function on_continue_clicked()
    if leaving or not save.has_save(1) then return end
    leaving = true
    sound.play("sounds/click.wav")
    save.load_game(1)
end

function on_settings_clicked()
    if leaving then return end
    leaving = true
    sound.play("sounds/click.wav")
    scene.transition_to("scenes/settings.owl", "fade_out", 0.3, 0.02, 0.02, 0.05)
end

function on_quit_clicked()
    scene.quit()
end
