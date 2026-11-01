-- The conversations of the tavern: a talker (tavern_patron.lua) leaves the path of its dialogue in the game state, this
-- script plays it in the modal dialogue window of huds/tavern.owlhud (the speaker in its title bar). The player stops
-- walking and looking while it is open; E shows the next line, the arrows and Enter (or the pad) pick an answer,
-- Escape or the close button ends it. The barkeeper serves a cider (an Event: its `serve` animation) and knows where
-- the cellar key lies (a Branch on `tavern_key`).
-- Demonstrates: dialogue.start / get_line / next / choose / stop, Branch and Event functions of a dialogue tree,
-- entity.set_first_person_enabled, animator.trigger, ui.set_focus on the answers, `tavern_talks` and
-- `tavern_choices` as probes for the scenarios.

properties = {
    { name = "player", type = "string", default = "TavernCamera" },
    { name = "barkeeper", type = "string", default = "Barkeeper" },
}

local conversation = nil
local hud = {}
local player_id = 0
local barkeeper_id = 0
local frame = 0
local started_frame = -1

function on_create()
    hud.panel = ui.find("DialoguePanel")
    hud.text = ui.find("DialogueText")
    hud.next = ui.find("DialogueNext")
    hud.answers = { ui.find("DialogueAnswer1"), ui.find("DialogueAnswer2"), ui.find("DialogueAnswer3") }
    hud.choices = {}
    ui.set_visible(hud.panel, false)
    player_id = scene.find_entity(player)
    barkeeper_id = scene.find_entity(barkeeper)
    gamestate.set("dialogue_open", 0)
    gamestate.set("tavern_talk_request", "")
    gamestate.set("tavern_choices", 0)
    gamestate.set("tavern_talks", gamestate.get("tavern_talks", 0))
    gamestate.set("tavern_ciders", gamestate.get("tavern_ciders", 0))
end

-- Function of the Branch of the barkeeper: the key of the cellar is found.
function has_cellar_key()
    return gamestate.get("tavern_key", 0) > 0
end

-- Function of the Event of the barkeeper: he serves a cider.
function serve_cider()
    if barkeeper_id ~= 0 then
        animator.trigger(barkeeper_id, "serve")
    end
    sound.play("sounds/raycast/treasure.wav")
    gamestate.set("tavern_ciders", gamestate.get("tavern_ciders", 0) + 1)
end

local function set_walking(enabled)
    if player_id ~= 0 then
        entity.set_first_person_enabled(player_id, enabled)
    end
end

local function close()
    conversation = nil
    ui.set_visible(hud.panel, false)
    gamestate.set("dialogue_open", 0)
    gamestate.set("tavern_choices", 0)
    -- The Escape that ends the conversation is not a pause (tavern.lua reads it).
    gamestate.set("tavern_closed_frame", frame)
    set_walking(true)
end

local function show()
    local line = conversation and dialogue.get_line(conversation)
    if line == nil then
        close()
        return
    end
    ui.set_visible(hud.panel, true)
    ui.set_window_title(hud.panel, line.speaker)
    ui.set_text(hud.text, line.text)
    hud.choices = line.choices
    gamestate.set("tavern_choices", #line.choices)
    ui.set_visible(hud.next, #line.choices == 0)
    for i = 1, 3 do
        local choice = line.choices[i]
        ui.set_visible(hud.answers[i], choice ~= nil)
        if choice ~= nil then
            ui.set_text(hud.answers[i], i .. ". " .. choice)
        end
    end
    ui.set_focus(#line.choices > 0 and hud.answers[1] or hud.next)
end

local function advance()
    if conversation ~= nil and #hud.choices == 0 then
        dialogue.next(conversation)
        show()
    end
end

local function answer(index)
    if conversation ~= nil and hud.choices[index] ~= nil then
        dialogue.choose(conversation, index)
        show()
    end
end

-- On Click Callbacks of the buttons of the dialogue window.
function on_tavern_next() advance() end
function on_tavern_answer_1() answer(1) end
function on_tavern_answer_2() answer(2) end
function on_tavern_answer_3() answer(3) end

-- On Close Callback of the window (its close button); on_update calls it on ui_cancel (Escape) too.
function on_tavern_closed()
    if conversation ~= nil then
        dialogue.stop(conversation)
    end
    close()
end

function on_update(_)
    frame = frame + 1
    gamestate.set("tavern_frame", frame)
    local request = gamestate.get("tavern_talk_request", "")
    if conversation == nil and request ~= "" then
        gamestate.set("tavern_talk_request", "")
        conversation = dialogue.start(request)
        if conversation ~= nil then
            started_frame = frame
            gamestate.set("dialogue_open", 1)
            gamestate.set("tavern_talks", gamestate.get("tavern_talks", 0) + 1)
            set_walking(false)
        end
        show()
        return
    end
    if conversation == nil then
        return
    end
    if input.is_action_just_pressed("ui_cancel") then
        on_tavern_closed()
    elseif input.is_action_just_pressed("interact") and frame ~= started_frame then
        advance()
    end
end
