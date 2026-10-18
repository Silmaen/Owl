-- Talker of the hub: a signpost or a villager that answers when the hero presses E near it.
-- Demonstrates: `Trigger.Type=Interaction` on an entity with no picture (its scale is the reach), script properties
--               that carry the content (`speaker`, `lines` separated by `|`, `follow` the tag of a character the trigger
--               goes with: the villagers walk, so the trigger copies their position every frame), a line picked at
--               random but never the same twice in a row (`math.random`), a speech shown in the HUD (`HubSpeech`, a
--               `UiText` with an outline and a box, `ui.set_text` / `ui.set_visible`) that hides itself after a few
--               seconds.
--
-- The last line is in gamestate (`hub_talk_who`, `hub_talk_text`), `hub_talk_count` counts them for the scenario test.
-- The speech belongs to the last talker (`hub_talk_owner`): the others leave it alone when their own timer ends.

properties = {
    { name = "speaker", type = "string", default = "" },
    { name = "lines", type = "string", default = "" },
    { name = "follow", type = "string", default = "" },
}

local SECONDS_PER_LINE, SECONDS_MIN = 0.06, 3.5   -- a longer line stays longer

local list = {}
local speech_id, followed_id = 0, 0
local last = 0
local left = 0

function on_create()
    gamestate.set("hub_talk_count", gamestate.get("hub_talk_count", 0))
    for line in string.gmatch(lines, "[^|]+") do
        list[#list + 1] = line
    end
    speech_id = ui.find("HubSpeech")
    if follow ~= "" then
        followed_id = scene.find_entity(follow)
    end
end

-- Bound by Trigger.Type=Interaction with CallbackName=on_talk.
function on_talk(player_id)
    if #list == 0 or speech_id == 0 then
        return
    end
    local pick = math.random(#list)
    if pick == last and #list > 1 then
        pick = pick % #list + 1
    end
    last = pick
    local text = speaker ~= "" and (speaker .. ": " .. list[pick]) or list[pick]
    ui.set_text(speech_id, text)
    ui.set_visible(speech_id, true)
    left = math.max(SECONDS_MIN, #text * SECONDS_PER_LINE)
    sound.set_volume(sound.play("sounds/hub/board.wav"), 0.35)
    gamestate.set("hub_talk_owner", entity_id)
    gamestate.set("hub_talk_who", speaker)
    gamestate.set("hub_talk_text", list[pick])
    gamestate.set("hub_talk_count", gamestate.get("hub_talk_count", 0) + 1)
end

function on_update(dt)
    if followed_id ~= 0 then
        local x, y = transform.get_position(followed_id)
        transform.set_position(entity_id, x, y, 0.6)
    end
    if left > 0 then
        left = left - dt
        if left <= 0 and gamestate.get("hub_talk_owner", 0) == entity_id then
            ui.set_visible(speech_id, false)
        end
    end
end
