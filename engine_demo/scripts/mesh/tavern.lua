-- Tavern scene script: the level of the tavern. The key of the cellar hides by the hearth; once it is taken, the
-- trapdoor opens on the cellar (scenes/tavern_cellar.owl), where the chest ends the level and the ladder climbs back. Escape (or Start) goes back
-- to the hub without completing it. The fire and the candles flicker through `LightFlicker` components, a
-- `FirstPersonController` walks the player. Coming from the hub, an intro cutscene (sequences/tavern_intro.owlseq, played by
-- the `SequencePlayer` of TavernIntro) shows the room then the hearth; Space skips it.
-- Demonstrates: a scene script, gamestate shared by the scripts of a level, ui.set_text, scene.transition_to,
-- sequence.play / sequence.stop and the events of a sequence.

properties = {
    { name = "hub_scene", type = "string", default = "scenes/world_map.owl" },
    { name = "player", type = "string", default = "TavernCamera" },
    { name = "cellar_return", type = "vec3", default = { 2.2, 1.6, 2.2 } },  -- where the cellar ladder leads, by the trapdoor
    { name = "cellar_return_yaw", type = "float", default = 2.4 },
    { name = "intro", type = "string", default = "TavernIntro" },
}

local objective_id = 0
local leaving = false
local shown = -1
local shown_message = ""
local intro_id = 0

function on_create()
    objective_id = ui.find("ObjectiveText")
    gamestate.set("tavern_message", "")
    -- Back up the cellar ladder: the player stands by the trapdoor, the key already taken.
    if gamestate.get("tavern_from_cellar", 0) > 0 then
        gamestate.set("tavern_from_cellar", 0)
        local player_id = scene.find_entity(player)
        if player_id ~= 0 then
            transform.set_position(player_id, cellar_return.x, cellar_return.y, cellar_return.z)
            transform.set_rotation(player_id, 0, cellar_return_yaw, 0)
        end
    else
        intro_id = scene.find_entity(intro)
        if intro_id ~= 0 and sequence.play(intro_id) then
            gamestate.set("tavern_intro", 1)
        end
    end
    ui.transition_fade_in(0.4)
end

function on_update(_)
    if intro_id ~= 0 and sequence.is_playing(intro_id) and input.is_action_just_pressed("jump") then
        sequence.stop(intro_id)
    end
    if leaving or ui.is_transition_active() then
        return
    end
    if input.is_action_just_pressed("pause") then
        leaving = true
        scene.transition_to(hub_scene, "fade_out", 0.3)
        return
    end
    local has_key = gamestate.get("tavern_key", 0)
    local message = gamestate.get("tavern_message", "")
    local state = has_key * 2 + (message ~= "" and 1 or 0)
    if state == shown and message == shown_message then
        return
    end
    shown = state
    shown_message = message
    if message ~= "" then
        ui.set_text(objective_id, message)
    elseif has_key > 0 then
        ui.set_text(objective_id, "Objective: open the cellar trapdoor")
    else
        ui.set_text(objective_id, "Objective: find the key of the cellar")
    end
end

-- Events of the intro sequence, then its end (the `onFinished` of the player, also met when Space skips it).
function on_intro_room()
    gamestate.set("tavern_intro_shot", 1)
    gamestate.set("tavern_message", "The tavern of the village. Space skips.")
end

function on_intro_hearth()
    gamestate.set("tavern_intro_shot", 2)
    gamestate.set("tavern_message", "The hearth keeps the room warm...")
end

function on_intro_key()
    gamestate.set("tavern_message", "...and something glints by the bucket.")
end

function on_intro_done()
    gamestate.set("tavern_intro", 0)
    gamestate.set("tavern_message", "")
end
