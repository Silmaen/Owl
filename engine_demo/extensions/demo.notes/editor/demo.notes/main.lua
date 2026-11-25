-- Owl Nest plugin of the demo.notes pack: design notes, pinned on entities and kept in `.owlnote` documents.
-- It adds one of each thing a plugin can add: a command, a panel, an inspector drawer, a document type and an
-- importer. Every change goes through an editor command, hence can be undone.

local NOTE = "demo.notes.Note"

-- Command: `demo.notes.pin entity=<entity> text="..."`, one undo step.
nest.command{name = "demo.notes.pin", description = "Pin a design note on an entity", handler = "pin_note"}
nest.argument{command = "demo.notes.pin", name = "entity", type = "entity", required = true,
              description = "Entity the note is pinned on."}
nest.argument{command = "demo.notes.pin", name = "text", type = "text", default = "To do",
              description = "Text of the note."}

function pin_note(args)
    local target = nest.entity{entity = args.entity, component = NOTE}
    if not target.exists then
        error("no entity " .. tostring(args.entity))
    end
    if not target.has then
        editor.component.add{entity = target.entity, component = NOTE}
    end
    editor.component.set_field{entity = target.entity, component = NOTE, field = "text", value = args.text}
    editor.component.set_field{entity = target.entity, component = NOTE, field = "done", value = false}
    return {entity = target.entity, description = "Pin a note on " .. target.name}
end

-- Panel: pins a note on the selected entity.
nest.panel{id = "demo.notes.panel", title = "Design Notes", handler = "draw_notes_panel"}

local draft = "To do"

function draw_notes_panel(args)
    if ui.section{label = "Pin a note"}.open then
        if args.selection == nil then
            ui.text{text = "Select an entity to pin a note on it.", dim = true}
        else
            ui.value{label = "Entity", value = args.selection_name}
            draft = ui.input{label = "Note", value = draft, tooltip = "Text of the note."}.value
            if ui.button{label = "Pin", tooltip = "Pin the note on the entity", primary = true}.clicked then
                editor.demo.notes.pin{entity = args.selection, text = draft}
            end
        end
    end
    if ui.section{label = "Pinned notes"}.open then
        -- Read only: the scene is read with `nest.*`, every change goes through a command.
        local pinned = nest.entities{component = NOTE, max = 20}
        ui.value{label = "Pinned", value = pinned.count}
        for index, id in ipairs(pinned.ids) do
            local note = nest.field{entity = id, component = NOTE, field = "text"}
            ui.value{label = pinned.names[index], value = note.value}
        end
    end
    if ui.section{label = "Scene"}.open then
        ui.value{label = "Entities", value = args.entity_count}
        ui.value{label = "State", value = args.playing and "Playing" or "Editing"}
    end
end

-- Inspector drawer of the Note component.
nest.drawer{component = NOTE, handler = "draw_note"}

local function set_note(args, field, value)
    editor.component.set_field{entity = args.entity, component = NOTE, field = field, value = value}
end

function draw_note(args)
    local text = ui.input{label = "Note", value = args.text, tooltip = "What is left to do on this entity."}
    if text.changed then
        set_note(args, "text", text.value)
    end
    local done = ui.checkbox{label = "Done", value = args.done}
    if done.changed then
        set_note(args, "done", done.value)
    end
end

-- Document type: a `.owlnote` file is a check list, one `- [ ] text` or `- [x] text` line per note.
nest.document{id = "demo.notes.document", extension = ".owlnote", title = "Design Notes",
              handler = "draw_note_document"}

local function read_notes(text)
    local notes = {}
    for line in string.gmatch(text, "[^\r\n]+") do
        local mark, label = string.match(line, "^%- %[([ x])%] (.*)$")
        if mark ~= nil then
            table.insert(notes, {done = mark == "x", text = label})
        end
    end
    return notes
end

local function write_notes(notes)
    local lines = {}
    for _, note in ipairs(notes) do
        table.insert(lines, string.format("- [%s] %s", note.done and "x" or " ", note.text))
    end
    return table.concat(lines, "\n") .. "\n"
end

local new_note = ""

function draw_note_document(args)
    local notes = read_notes(args.text)
    local changed = false
    if ui.section{label = args.name}.open then
        if #notes == 0 then
            ui.text{text = "No note yet.", dim = true}
        end
        for _, note in ipairs(notes) do
            local box = ui.checkbox{label = note.text, value = note.done}
            if box.changed then
                note.done = box.value
                changed = true
            end
        end
    end
    if ui.section{label = "Edit"}.open then
        new_note = ui.input{label = "New note", value = new_note}.value
        if ui.button{label = "Add", tooltip = "Add the note to the list", primary = true}.clicked
            and new_note ~= "" then
            table.insert(notes, {done = false, text = new_note})
            new_note = ""
            changed = true
        end
        if ui.button{label = "Remove done", tooltip = "Remove the notes that are done"}.clicked then
            local kept = {}
            for _, note in ipairs(notes) do
                if not note.done then
                    table.insert(kept, note)
                end
            end
            changed = changed or #kept ~= #notes
            notes = kept
        end
    end
    if changed then
        editor.plugin.document_set{path = args.path, text = write_notes(notes)}
    end
end

-- Importer: a `.txt` file becomes a `.owlnote` beside it, one note per line.
nest.importer{id = "demo.notes.text", extension = ".txt", description = "Text file to design notes",
              handler = "import_notes"}

function import_notes(args)
    local notes = {}
    for line in string.gmatch(args.text, "[^\r\n]+") do
        if string.match(line, "%S") then
            table.insert(notes, {done = false, text = line})
        end
    end
    return {path = args.name .. ".owlnote", text = write_notes(notes)}
end
