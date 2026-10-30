-- The wizard of the hub: E near him starts the conversation of dialogues/wizard.owldlg, drawn by the modal dialogue
-- window of huds/world_map.owlhud (the speaker in its title bar).
-- Demonstrates: a dialogue tree (`dialogue.start`, `dialogue.get_line`, `dialogue.next`, `dialogue.choose`,
--               `dialogue.stop`) with a Branch (`is_portal_awake`), an answer offered on a condition (`has_met_wizard`),
--               an Event (`cast_spell` rings the bell), a voice sound on a line, texts translated through the string
--               tables (keys `dialogue.wizard.*` of localisation.yml), answers picked with the mouse or the
--               gamepad, E for the next line, Escape or the close button to leave.
--
-- `dialogue_open` freezes the hero (world_player.lua); `wizard_talks` counts the conversations, `wizard_spells` the
-- spells and `wizard_choices` is the number of answers offered, for the scenario test.

properties = {
    { name = "dialogue_path", type = "string", default = "dialogues/wizard.owldlg" },
    { name = "follow", type = "string", default = "Wizard" },
}

local conversation = nil
local hud = {}
local followed_id = 0
local frame = 0
local started_frame = -1

function on_create()
    hud.panel = ui.find("DialoguePanel")
    hud.text = ui.find("DialogueText")
    hud.next = ui.find("DialogueNext")
    hud.answers = { ui.find("DialogueAnswer1"), ui.find("DialogueAnswer2"), ui.find("DialogueAnswer3") }
    hud.choices = {}
    ui.set_visible(hud.panel, false)
    followed_id = scene.find_entity(follow)
    gamestate.set("dialogue_open", 0)
    gamestate.set("wizard_choices", 0)
    gamestate.set("wizard_talks", gamestate.get("wizard_talks", 0))
    gamestate.set("wizard_spells", gamestate.get("wizard_spells", 0))
end

-- Condition of the Branch: true once every level is done.
function is_portal_awake()
    return gamestate.get("levels_done", 0) >= 5
end

-- Condition of the "Show me a spell" answer: offered from the second conversation on.
function has_met_wizard()
    return gamestate.get("wizard_talks", 0) > 1
end

-- Function of the Event node.
function cast_spell()
    sound.play("sounds/hub/bell.wav")
    gamestate.set("wizard_spells", gamestate.get("wizard_spells", 0) + 1)
end

local function close()
    conversation = nil
    ui.set_visible(hud.panel, false)
    gamestate.set("dialogue_open", 0)
    gamestate.set("wizard_choices", 0)
    -- The Escape or the E that ends the conversation is not a pause nor a new talk (world_player.lua reads it).
    gamestate.set("board_closed_frame", gamestate.get("board_frame", 0))
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
    if line.sound ~= "" then
        sound.set_volume(sound.play(line.sound), 0.35)
    end
    hud.choices = line.choices
    gamestate.set("wizard_choices", #line.choices)
    ui.set_visible(hud.next, #line.choices == 0)
    for i = 1, 3 do
        local choice = line.choices[i]
        ui.set_visible(hud.answers[i], choice ~= nil)
        if choice ~= nil then
            ui.set_text(hud.answers[i], i .. ". " .. choice)
        end
    end
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

-- Bound by Trigger.Type=Interaction with CallbackName=on_talk: E starts the conversation. While the window is shown the
-- engine holds the interaction triggers back, so on_update reads the next presses of `interact`.
function on_talk(player_id)
    if conversation ~= nil then
        return
    end
    gamestate.set("wizard_talks", gamestate.get("wizard_talks", 0) + 1)
    conversation = dialogue.start(dialogue_path)
    if conversation ~= nil then
        started_frame = frame
        gamestate.set("dialogue_open", 1)
    end
    show()
end

-- On Click Callbacks of the buttons of the dialogue panel.
function on_wizard_next() advance() end
function on_wizard_answer_1() answer(1) end
function on_wizard_answer_2() answer(2) end
function on_wizard_answer_3() answer(3) end

-- On Close Callback of the window (its close button); on_update calls it on ui_cancel (Escape) too.
function on_wizard_closed()
    if conversation ~= nil then
        dialogue.stop(conversation)
    end
    close()
end

function on_update(dt)
    frame = frame + 1
    if followed_id ~= 0 then
        local x, y = transform.get_position(followed_id)
        transform.set_position(entity_id, x, y, 0.6)
    end
    if conversation == nil then
        return
    end
    if input.is_action_just_pressed("ui_cancel") then
        on_wizard_closed()
    elseif input.is_action_just_pressed("interact") and frame ~= started_frame then
        advance()
    end
end
