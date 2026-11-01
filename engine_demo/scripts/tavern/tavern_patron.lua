-- Somebody of the tavern who answers on E (the barkeeper, the server, a patron at a table): asks the dialogue manager
-- of the scene (tavern_dialogues.lua) to play the conversation of `dialogue_path`.
-- Demonstrates: an Interaction trigger in 3D (the first-person player uses what it aims at), one script for every
-- talker through its properties, the game state as a request between two scripts.

properties = {
    { name = "dialogue_path", type = "string", default = "dialogues/barkeeper.owldlg" },
}

-- Bound by Trigger.Type=Interaction with CallbackName=on_talk.
function on_talk(_)
    if gamestate.get("dialogue_open", 0) == 0 then
        gamestate.set("tavern_talk_request", dialogue_path)
    end
end
