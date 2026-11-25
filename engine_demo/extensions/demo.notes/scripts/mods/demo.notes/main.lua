-- Entry script of the demo.notes pack: the Note component the Owl Nest plugin of the pack pins on entities. A note
-- is for the designers, the game does nothing with it.

components = {
    ["demo.notes.Note"] = {
        { name = "text", type = "string", default = "To do" },
        { name = "done", type = "bool", default = false },
    },
}
