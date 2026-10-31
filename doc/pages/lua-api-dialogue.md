# Lua API: Dialogues {#page-lua-api-dialogue}

[TOC]

Generated from the binding registry, do not edit by hand: see the [Lua API reference](lua-api.md) for the conventions and the other pages.

## `dialogue`

Conversations over the `.owldlg` dialogues: lines, choices, Lua conditions and actions.

| Function                                                  | Returns                 | Description                                                                                                                                                                                               |
|-----------------------------------------------------------|-------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `dialogue.start(dialogue: string)`                        | `conversation: integer` | Start a conversation over a `.owldlg` dialogue and go to its first line; its conditions and actions are global functions of the calling script. Nothing when the dialogue cannot be read or ends at once. |
| `dialogue.get_line(conversation: integer)`                | `line: table`           | What a conversation shows: `{kind, speaker, text, sound, choices}`, the texts in the language of the game, `choices` the offered answers of a Choice (empty on a Line); nothing once it ended.            |
| `dialogue.next(conversation: integer)`                    | `running: boolean`      | Leave the current line for the next one; does nothing on a Choice. False once the conversation ended.                                                                                                     |
| `dialogue.choose(conversation: integer, choice: integer)` | `running: boolean`      | Pick an offered answer of the current Choice, from 1. False once the conversation ended.                                                                                                                  |
| `dialogue.is_running(conversation: integer)`              | `running: boolean`      | Whether a conversation still shows a line.                                                                                                                                                                |
| `dialogue.stop(conversation: integer)`                    |                         | End a conversation where it is.                                                                                                                                                                           |
