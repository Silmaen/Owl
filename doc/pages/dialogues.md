# Dialogues {#page-dialogues}

[TOC]

A dialogue is a branching conversation: who speaks, what they say, the answers the player can pick, and the tests and
actions a script runs on the way. It is a `.owldlg` asset, edited as a graph in Owl Nest and played from Lua; the game
draws it with its own HUD. Plan: [Gameplay systems](design/gameplay-systems.md).

## The dialogue file {#dialogues-file}

A YAML file: a list of nodes, each with a stable id and its place in the graph, and the bends of the links. A node
leads to the next one through its outputs; an output left unlinked ends the conversation.

| Kind     | Does                                                                        | Outputs                          |
|----------|-----------------------------------------------------------------------------|----------------------------------|
| `Start`  | Entry of the conversation                                                   | One                              |
| `Line`   | A speaker says a text (and plays an optional sound), then goes on           | One                              |
| `Choice` | A speaker says a text, the player picks an answer                           | One per answer                   |
| `Branch` | A Lua condition picks the way                                               | Two: true, false                 |
| `Event`  | Calls a Lua function, then goes on                                          | One                              |
| `End`    | Ends the conversation                                                       | None                             |

- A text is written with an optional **localisation key**; see [Localisation](#dialogues-localisation).
- Each answer of a Choice has its own text and an optional Lua **condition**: the answer is offered only when the
  function returns true.
- The condition of a Branch and the function of an Event are names of **global functions of the script** that calls
  `dialogue.start`.
- A dialogue has one Start node. The editor reports as problems a missing or second Start, a link to a missing node,
  a Choice without answer, a Branch or an Event without function and a node no path reaches.

## Editing in Owl Nest {#dialogues-editor}

Create one with **New Dialogue** in the Content Browser (a Start node linked to an End node), open it with a
double-click. The tab is a graph on the node canvas; the **Nodes** and **Node** panels replace the Hierarchy and
Properties panels while it is the active document.

- **Graph**: right-click the canvas to add a node of any kind, drag a link from an output to a node to connect it
  (dropping on empty canvas unlinks the output). The pins are execution chevrons and each kind has the colour of its
  glyph, in the graph and the Nodes panel. Right-click a link for its menu: **Add Waypoint**, a **Colour** (the link and
  its pins take it, until the link is removed) and **Delete Link**; the bends and the colour are kept with the dialogue
  (`routes:`). Click or box-select nodes, drag them to move (on the grid with **Snap to Grid**, the magnet of the
  toolbar), `Delete` removes, `Ctrl+D` duplicates, `Home` frames the graph.
- **Nodes panel**: every node with its kind and text, and the problems of the graph; a click selects the node in the
  graph.
- **Node panel**: for a Line or a Choice, the speaker, the text and their localisation keys, with the translation
  shown under each, and a sound dropped from the Content Browser; for a Branch the condition, for an Event the
  function; for a Choice its answers, each with a text, a key and a condition, added, moved up or down and removed.
- **Toolbar**: one button per node kind adds it beside the selected node, linked from it; duplicate, delete,
  **Preview** and a badge counting the problems (a click selects the first one). The ribbon tab has **Save**,
  **Close**, **Frame All**, **Duplicate** and **Preview**.
- **Preview**: a pane plays the dialogue from its Start. The conditions the dialogue calls are boxes to tick by hand,
  the actions it reaches are listed as called, and the lines, answers and **Next** button follow the graph.
- Every gesture is a `dialogue.*` command ([Console](editor_console.md)), one undo step of the tab. **Save**
  (`Ctrl+S`) writes the file; the next conversations read it again.

## Playing from Lua {#dialogues-lua}

A conversation is a number. `dialogue.start` returns it, the script shows what `dialogue.get_line` gives and moves
on with `dialogue.next` or `dialogue.choose` (answers count from 1). Reference: [Dialogues](lua-api-dialogue.md).

```lua
local conversation = nil
local hud = {}

function on_create()
    hud.panel = ui.find("DialoguePanel")
    hud.speaker = ui.find("DialogueSpeaker")
    hud.text = ui.find("DialogueText")
    hud.next = ui.find("DialogueNext")
    hud.answers = {ui.find("Answer1"), ui.find("Answer2"), ui.find("Answer3")}
    hud.answer_texts = {ui.find("Answer1Text"), ui.find("Answer2Text"), ui.find("Answer3Text")}
    ui.set_visible(hud.panel, false)
end

local function show()
    local line = conversation and dialogue.get_line(conversation)
    if line == nil then
        ui.set_visible(hud.panel, false)
        conversation = nil
        return
    end
    ui.set_visible(hud.panel, true)
    ui.set_text(hud.speaker, line.speaker)
    ui.set_text(hud.text, line.text)
    ui.set_visible(hud.next, #line.choices == 0)
    for i = 1, 3 do
        local choice = line.choices[i]
        ui.set_visible(hud.answers[i], choice ~= nil)
        if choice ~= nil then ui.set_text(hud.answer_texts[i], choice) end
    end
end

function has_key() return gamestate.get("has_key", false) end   -- condition of a Branch or an answer
function give_reward() gamestate.set("reward", 1) end            -- function of an Event

function on_interact()
    conversation = dialogue.start("dialogues/innkeeper.owldlg")   -- nil when it ends at once
    show()
end

function on_next_clicked()          -- On Click Callback of the Next button
    dialogue.next(conversation)
    show()
end

local function answer(index)
    dialogue.choose(conversation, index)
    show()
end

function on_answer_1() answer(1) end  -- On Click Callback of each answer button
function on_answer_2() answer(2) end
function on_answer_3() answer(3) end
```

- `dialogue.get_line` returns `{kind, speaker, text, sound, choices}` in the language of the game; `choices` lists the
  answers currently offered, as texts, and is empty on a Line. It returns nothing once the conversation
  ended.
- The conditions and actions are looked up in the script that called `dialogue.start`.
- `dialogue.is_running` tells whether a conversation still shows a line, `dialogue.stop` ends it where it is. The
  conversations stop when Play stops.
- The HUD is the game's: `ui.find` the widgets once and fill them with `ui.set_text`, `ui.set_visible`
  ([UI functions](lua-api-ui.md)). A `sound` of the line is played by the script, with the `sound` table.
- The wizard of the Engine Demo hub is a complete example: `engine_demo/scripts/hub/hub_wizard.lua` with
  `dialogues/wizard.owldlg` and the dialogue panel of `huds/world_map.owlhud`.

## Localisation {#dialogues-localisation}

The speaker and the text of a node, and the text of each answer, carry an optional key of the string tables
(`localisation.yml`, see [Languages](languages.md)). `get_line` gives the translation in the language of the game; a
text without key, or whose key has no translation, is given as written. The Node panel shows the translation of the
language of the editor under each key.

## Export {#dialogues-export}

A dialogue named in a script (a quoted `.owldlg` path) or in a string property of a script is packed with the game,
with the sounds its nodes play.
