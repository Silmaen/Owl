-- Main Menu controller
-- Demonstrates: UI button callbacks, scene.transition_to, gamestate, save system,
--               settings, log, entity visibility, UIImage, UIPanel,
--               save.load_game, save.list_saves, save.delete_save,
--               ui.transition_fade_in/out, ui.set_button_enabled, ui.set_visible,
--               input.get_mouse_x, input.get_mouse_y, ui.get_text

local pending_load_save = false
local save_info_id = 0
local delete_save_id = 0
local continue_id = 0
local mouse_label_id = 0

function on_create()
    log.info("Main Menu loaded")
    ui.transition_fade_in(0.5)

    continue_id = scene.find_entity("ContinueButton")
    save_info_id = scene.find_entity("SaveInfoText")
    delete_save_id = scene.find_entity("DeleteSaveButton")
    mouse_label_id = scene.find_entity("MousePosLabel")

    -- Hide the legacy ContinueButton — the primary PlayButton now toggles
    -- between "Démarrer" and "Continuer" depending on whether an in-game
    -- pause snapshot exists in gamestate.
    if continue_id ~= 0 then
        ui.set_visible(continue_id, false)
    end

    -- Update the PlayButton label according to the pause-continue snapshot.
    local play_id = scene.find_entity("PlayButton")
    if play_id ~= 0 then
        local has_resume = gamestate.get("has_continue", false)
        if has_resume then
            ui.set_text(play_id, "Continuer")
        else
            ui.set_text(play_id, "Démarrer")
        end
    end

    refresh_save_info()
end

function refresh_save_info()
    -- Use save.list_saves to get save metadata.
    local saves = save.list_saves()
    local has_save_1 = save.has_save(1)

    if has_save_1 then
        local prev_score = gamestate.get("score", 0)
        log.info("Save found in slot 1! Previous score: " .. prev_score)

        -- Display save info
        if save_info_id ~= 0 then
            local info_text = "Save slot 1 — Score: " .. prev_score
            ui.set_text(save_info_id, info_text)
            -- Verify text was set correctly using ui.get_text
            local readback = ui.get_text(save_info_id)
            log.trace("Save info displayed: " .. readback)
        end

        -- Enable Continue button and show Delete Save button
        if continue_id ~= 0 then
            ui.set_button_enabled(continue_id, true)
        end
        if delete_save_id ~= 0 then
            ui.set_visible(delete_save_id, true)
        end
    else
        log.info("No save found. Starting fresh.")
        gamestate.set("score", 0)

        if save_info_id ~= 0 then
            ui.set_text(save_info_id, "No save data")
        end
        if continue_id ~= 0 then
            ui.set_button_enabled(continue_id, false)
        end
        if delete_save_id ~= 0 then
            ui.set_visible(delete_save_id, false)
        end
    end
end

function on_update(dt)
    -- Save-load goes through the engine-side `save` table which still uses the
    -- deferred-load pattern (no per-scene transition for it yet) — keep the
    -- "wait for fade then fire" handler. Plain scene loads use
    -- `scene.transition_to`, fully orchestrated engine-side.
    if pending_load_save then
        if not ui.is_transition_active() then
            save.load_game(1)
            pending_load_save = false
        end
        return
    end

    -- Display mouse position and button state (demonstrates input.get_mouse_x/y/is_mouse_button_pressed)
    if mouse_label_id ~= 0 then
        local mx = input.get_mouse_x()
        local my = input.get_mouse_y()
        local lmb = input.is_mouse_button_pressed(0)
        local indicator = lmb and " [click]" or ""
        ui.set_text(mouse_label_id, string.format("Mouse: %d, %d%s", math.floor(mx), math.floor(my), indicator))
    end
end

function on_play_clicked()
    -- Either resume a paused session ("Continuer") or start a fresh run ("Démarrer").
    local has_resume = gamestate.get("has_continue", false)
    if has_resume then
        local target = gamestate.get("continue_scene", "scenes/world_map.owl")
        log.info("Continue clicked — resuming '" .. target .. "'")
        sound.play("sounds/start.wav")
        -- Position is restored by the destination scene's player script
        -- (world_player.lua / platformer_player.lua read continue_x/y/z).
        gamestate.set("has_continue", false)
        scene.transition_to(target, "fade_out", 0.3)
    else
        log.info("Start clicked — fresh run, loading world map.")
        sound.play("sounds/start.wav")
        gamestate.clear()
        scene.transition_to("scenes/world_map.owl", "fade_out", 0.3)
    end
end

function on_continue_clicked()
    log.info("Continue button clicked! Loading save...")
    sound.play("sounds/start.wav")
    pending_load_save = true
    ui.transition_fade_out(0.3)
end

function on_settings_clicked()
    log.info("Settings button clicked!")
    scene.transition_to("scenes/settings_menu.owl", "fade_out", 0.3)
end

function on_delete_save_clicked()
    if not save.has_save(1) then
        log.error("Cannot delete: no save in slot 1")
        return
    end
    log.info("Deleting save slot 1")
    save.delete_save(1)
    refresh_save_info()
end

function on_quit_clicked()
    log.info("Quit button clicked!")
    scene.quit()
end
