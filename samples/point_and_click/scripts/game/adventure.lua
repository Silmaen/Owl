-- The adventure rules shared by the rooms of The Moon Lantern: the cursor and its action, the hotspots, the hero
-- walking to them, the inventory (picked, combined, used), the hero's remarks, the dialogues and the pause window.
--
-- A room script requires it in `on_create` and calls `adventure.start(room)` with:
--   name      the room (`cottage`), the prefix of its texts (`name.<room>.<id>`, `look.<room>.<id>`);
--   hotspots  strings "id verb [target]" from the room's properties, `verb` one of look, pick, use, talk, exit (its
--             target the name of a string property of the room holding the scene it leads to); each
--             hotspot is the entity `Hotspot_<id>` (its position and scale are the rectangle, edited in Owl Nest) and
--             its child `Stand_<id>`, where the hero stands to act on it;
--   act       function(id, verb, item) of the room, called once the hero stands there: true when it handled the
--             action (an unhandled look says `look.<room>.<id>`, anything else a polite refusal);
--   highlights  optional {id = entity name}: the sprite glowing under the cursor when it is not `Prop_<id>`.
-- The room calls `adventure.update(dt)` from `on_update`. The rest of the game reads and changes the state through
-- `adventure.has`, `give`, `take`, `flag`, `set_flag`, `say`, `talk`, `go` (all in `gamestate`, so a save keeps them).
--
-- Controls: the mouse moves the cursor, the left button does the action the cursor shows (walk, look, pick, use,
-- talk, go), the right button looks. The keyboard (arrows) and the left stick move the cursor too; Tab / E and Q, pad
-- RB and LB jump it from hotspot to hotspot; Enter or pad A acts, L or pad X looks. I, pad Y or the bag button open the
-- inventory: click an object to take it in hand (it becomes the cursor, the bag stays open), click another object of
-- the bag to combine them (the result appears in the bag) or a hotspot of the room to use it there (a click on the
-- room closes the bag); the right button puts it back, I, Escape or the bag button close the bag. Escape, P or pad
-- Start pause. A hotspot whose sprite is in the room (`Prop_<id>`, or the `highlights` of the room) glows under the
-- cursor. The system cursor is hidden while the game draws its own.

local A = {}

local STRIDE = 0.12               -- distance under which the hero is at his stand point
local CURSOR_SIZE = 32            -- cursor size, in units of the cursor layer (720 high)
local FRAMES = { walk = 0, look = 1, use = 2, talk = 3, pick = 4, exit_left = 5, exit_right = 6, pointer = 7 }
local ITEMS = { "jar", "firefly_jar", "berries", "crust", "pie", "moonstone" }
local SLOTS = 6

local room = nil
local ids = {}
local hotspots = {}
local held = nil                  -- the object in hand
local pending = nil               -- {hotspot, verb, item}: the action the hero walks to
local speech_timer = 0
local conversation = nil
local paused = false
local inventory_open = false
local inventory_focus = 1
local gamepad_mode = false
local cursor_px, cursor_py = 640, 360
local last_mouse_x, last_mouse_y = -1, -1
local hover = nil
local busy_timer = 0              -- seconds left of an animation that holds the controls
local after_busy = nil
local glow = { entity = 0, r = 1, g = 1, b = 1, a = 1, clock = 0 } -- the sprite under the cursor and its own tint
local cursor_hidden = false

-- -----------------------------------------------------------------------------------------------------------------
-- State kept in gamestate.

local function inventory()
    local list = {}
    for item in string.gmatch(gamestate.get("inventory", ""), "[^,]+") do
        list[#list + 1] = item
    end
    return list
end

local function store(list)
    gamestate.set("inventory", table.concat(list, ","))
    gamestate.set("inventory_count", #list)
end

---@param item any
---@return any result
function A.has(item)
    for _, owned in ipairs(inventory()) do
        if owned == item then return true end
    end
    return false
end

function A.give(item)
    if A.has(item) then return end
    local list = inventory()
    list[#list + 1] = item
    store(list)
    sound.play("sounds/pickup.wav")
    A.refresh_inventory()
end

---@param item any
function A.take(item)
    local list = inventory()
    for index, owned in ipairs(list) do
        if owned == item then
            table.remove(list, index)
            break
        end
    end
    store(list)
    if held == item then held = nil end
    A.refresh_inventory()
end

---@param name any
---@return any result
function A.flag(name)
    return gamestate.get("flag_" .. name, false)
end

---@param name any
---@param value any
function A.set_flag(name, value)
    gamestate.set("flag_" .. name, value ~= false)
end

-- -----------------------------------------------------------------------------------------------------------------
-- Texts.

local function name_of(hotspot)
    return tr("name." .. room.name .. "." .. hotspot.id)
end

local function item_name(item)
    return tr("item." .. item)
end

function A.say(key, ...)
    local text = tr(key)
    if select("#", ...) > 0 then text = string.format(text, ...) end
    ui.set_text(ids.SpeechText, text)
    ui.set_visible(ids.SpeechPanel, true)
    speech_timer = 1.6 + #text / 16
    gamestate.set("last_remark", key)
end

local function hush()
    speech_timer = 0
    ui.set_visible(ids.SpeechPanel, false)
end

-- -----------------------------------------------------------------------------------------------------------------
-- Inventory.

function A.refresh_inventory()
    local list = inventory()
    for slot = 1, SLOTS do
        local item = list[slot]
        ui.set_visible(ids["SlotIcon" .. slot], item ~= nil)
        if item ~= nil then
            for frame, known in ipairs(ITEMS) do
                if known == item then ui.set_image_frame(ids["SlotIcon" .. slot], frame - 1) end
            end
        end
        ui.set_button_enabled(ids["Slot" .. slot], item ~= nil)
    end
end

local function set_inventory(open)
    inventory_open = open
    ui.set_visible(ids.InventoryPanel, open)
    gamestate.set("inventory_open", open)
    if open then
        hush()
        A.refresh_inventory()
        if #inventory() > 0 then
            inventory_focus = math.min(inventory_focus, #inventory())
            ui.set_focus(ids["Slot" .. inventory_focus])
        end
    end
end

local function hold(item)
    held = item
    gamestate.set("held", item or "")
end

-- Two objects combined: the room's recipes first, then the game's.
local RECIPES = {
    { "berries", "crust", "pie", "say.combine.pie" },
}

local function combine(a, b)
    for _, recipe in ipairs(RECIPES) do
        if (recipe[1] == a and recipe[2] == b) or (recipe[1] == b and recipe[2] == a) then
            A.take(a)
            A.take(b)
            A.give(recipe[3])
            sound.play("sounds/combine.wav")
            ui.show_subtitle(tr("subtitle.pie"), 1.5)
            hold(nil)
            A.say(recipe[4])
            return
        end
    end
    A.say("say.combine.no")
end

-- An object of the bag clicked: taken in hand, combined with the one in hand, or put back; the bag stays open.
local function click_slot(slot)
    local item = inventory()[slot]
    if item == nil then return end
    sound.play("sounds/click.wav")
    if held ~= nil and held ~= item then
        combine(held, item)
        A.refresh_inventory()
    elseif held == item then
        hold(nil)
    else
        hold(item)
    end
end

for slot = 1, SLOTS do
    _G["on_slot_" .. slot] = function() click_slot(slot) end
end

function on_bag_clicked()
    set_inventory(not inventory_open)
end

-- -----------------------------------------------------------------------------------------------------------------
-- Dialogues.

local function show_line()
    local line = conversation and dialogue.get_line(conversation)
    if line == nil then
        conversation = nil
        ui.set_visible(ids.DialoguePanel, false)
        gamestate.set("talking", false)
        return
    end
    ui.set_visible(ids.DialoguePanel, true)
    ui.set_text(ids.DialogueSpeaker, line.speaker)
    ui.set_text(ids.DialogueText, line.text)
    ui.set_visible(ids.DialogueNext, #line.choices == 0)
    for index = 1, 3 do
        local choice = line.choices[index]
        ui.set_visible(ids["Answer" .. index], choice ~= nil)
        if choice ~= nil then ui.set_text(ids["Answer" .. index .. "Text"], choice) end
    end
    ui.set_focus(#line.choices > 0 and ids.Answer1 or ids.DialogueNext)
    if room.on_line ~= nil then room.on_line(line) end
end

function A.talk(path)
    hush()
    conversation = dialogue.start(path)
    gamestate.set("talking", conversation ~= nil)
    show_line()
end

function A.is_talking()
    return conversation ~= nil
end

function on_dialogue_next()
    if conversation == nil then return end
    dialogue.next(conversation)
    show_line()
end

local function answer(index)
    if conversation == nil then return end
    dialogue.choose(conversation, index)
    show_line()
end

function on_answer_1() answer(1) end
function on_answer_2() answer(2) end
function on_answer_3() answer(3) end

-- -----------------------------------------------------------------------------------------------------------------
-- Pause window.

local function set_paused(value)
    paused = value
    ui.set_visible(ids.PauseWindow, value)
    gamestate.set("paused", value)
    if value then
        hush()
        ui.set_focus(ids.PauseResume)
    end
end

function on_pause_resume() set_paused(false) end

function on_pause_save()
    save.save_game(1)
    A.say("say.saved")
    set_paused(false)
end

function on_pause_title()
    set_paused(false)
    save.save_game(1)
    scene.transition_to("scenes/title.owl", "fade_out", 0.6, 0.02, 0.02, 0.05)
end

-- -----------------------------------------------------------------------------------------------------------------
-- Leaving the room.

function A.go(target)
    if A.leaving then return end
    A.leaving = true
    gamestate.set("entry", room.name)
    sound.play("sounds/door.wav")
    ui.show_subtitle(tr("subtitle.door"), 1.5)
    scene.transition_to(target, "fade_out", 0.5, 0.02, 0.02, 0.05)
end

-- -----------------------------------------------------------------------------------------------------------------
-- Hotspots and the cursor.

local function parse(lines)
    local out = {}
    for _, line in ipairs(lines) do
        local words = {}
        for word in string.gmatch(line, "%S+") do words[#words + 1] = word end
        local id = words[1]
        local entity = scene.find_entity("Hotspot_" .. id)
        if entity ~= 0 then
            -- The target of an exit names a string property of the room holding the scene path (packed by the export).
            local target = words[3] and (_G[words[3]] or words[3])
            local glows = (room.highlights or {})[id] or ("Prop_" .. id)
            out[#out + 1] = { id = id, verb = words[2] or "look", target = target, entity = entity,
                              stand = scene.find_entity("Stand_" .. id), prop = scene.find_entity(glows),
                              enabled = true }
        end
    end
    return out
end

---@param id any
---@param enabled any
function A.set_hotspot_enabled(id, enabled)
    for _, hotspot in ipairs(hotspots) do
        if hotspot.id == id then hotspot.enabled = enabled end
    end
end

local function contains(hotspot, x, y)
    local cx, cy = transform.get_world_position(hotspot.entity)
    local w, h = transform.get_scale(hotspot.entity)
    return math.abs(x - cx) <= w / 2 and math.abs(y - cy) <= h / 2
end

local function hotspot_at(x, y)
    for _, hotspot in ipairs(hotspots) do
        if hotspot.enabled and contains(hotspot, x, y) then return hotspot end
    end
    return nil
end

-- The world point under a pixel of the viewport (the camera is orthographic, `ortho` units high).
local function to_world(px, py)
    local vw, vh = scene.get_viewport_size()
    if vh <= 0 then return 0, 0 end
    local cx, cy = transform.get_world_position(ids.MainCamera)
    local unit = room.ortho / vh
    return cx + (px - vw / 2) * unit, cy - (py - vh / 2) * unit
end

local function to_pixel(x, y)
    local vw, vh = scene.get_viewport_size()
    local cx, cy = transform.get_world_position(ids.MainCamera)
    local unit = room.ortho / math.max(1, vh)
    return vw / 2 + (x - cx) / unit, vh / 2 - (y - cy) / unit
end

local function verb_of(hotspot)
    if hotspot == nil then return "walk" end
    if hotspot.verb == "exit" then
        local x = transform.get_world_position(hotspot.entity)
        return x < 0 and "exit_left" or "exit_right"
    end
    return hotspot.verb
end

-- The sprite of a hotspot glows while the cursor is over it; the one it left gets its own tint back.
local function set_glow(hotspot, dt)
    local target = hotspot and hotspot.prop or 0
    if target ~= glow.entity then
        if glow.entity ~= 0 then entity.set_tint(glow.entity, glow.r, glow.g, glow.b, glow.a) end
        glow.entity = target
        glow.clock = 0
        if target ~= 0 then glow.r, glow.g, glow.b, glow.a = entity.get_tint(target) end
        if glow.r == nil then glow.entity, glow.r, glow.g, glow.b, glow.a = 0, 1, 1, 1, 1 end
    end
    target = glow.entity
    if target == 0 then return end
    glow.clock = glow.clock + dt
    local boost = 1.45 + 0.15 * math.sin(glow.clock * 5)
    entity.set_tint(target, glow.r * boost, glow.g * boost, glow.b * boost * 0.9, glow.a)
end

-- The object in hand is the cursor: the HUD tool cursor of its picture (over the bag too) with the mouse, the item
-- sprite at the pointer with the keyboard and the gamepad.
local function show_held(active)
    for _, item in ipairs(ITEMS) do
        local cursor = ids["Held_" .. item]
        if cursor ~= nil and cursor ~= 0 then ui.set_tool_cursor(cursor, active and item == held and not gamepad_mode) end
    end
end

local function update_cursor(dt)
    local vw, vh = scene.get_viewport_size()
    if vh <= 0 then return end
    local scale = 720 / vh
    local sx, sy = cursor_px * scale, (vh - cursor_py) * scale
    ui.set_visible(ids.BagButton, not paused and conversation == nil)
    local blocked = paused or conversation ~= nil or inventory_open or ui.is_mouse_over_ui()
    local holding = held ~= nil and not paused and conversation == nil
    show_held(holding)
    ui.set_visible(ids.Cursor, not blocked and held == nil)
    ui.set_visible(ids.ItemCursor, not blocked and held ~= nil and gamepad_mode)
    ui.set_visible(ids.Pointer, gamepad_mode and not blocked)
    -- The system cursor shows only where the game draws none of its own (a menu, a button without an object in hand).
    local hide = holding or gamepad_mode or not blocked
    if hide ~= cursor_hidden then
        cursor_hidden = hide
        input.set_cursor_visible(not hide)
    end
    transform.set_position(ids.Pointer, sx + CURSOR_SIZE / 2, sy - CURSOR_SIZE / 2, 0.2)
    transform.set_position(ids.Cursor, sx + CURSOR_SIZE * 1.1, sy - CURSOR_SIZE * 1.1, 0.1)
    transform.set_position(ids.ItemCursor, sx + CURSOR_SIZE * 1.1, sy - CURSOR_SIZE * 1.1, 0.1)
    if blocked then
        set_glow(nil, dt)
        ui.set_text(ids.HoverLabel, (inventory_open and held ~= nil) and item_name(held) or "")
        return
    end
    local wx, wy = to_world(cursor_px, cursor_py)
    hover = hotspot_at(wx, wy)
    set_glow(hover, dt)
    local verb = verb_of(hover)
    isometric.set_sprite_frame(ids.Cursor, FRAMES[verb])
    if held ~= nil then
        for frame, known in ipairs(ITEMS) do
            if known == held then isometric.set_sprite_frame(ids.ItemCursor, frame - 1) end
        end
    end
    local label = ""
    if held ~= nil then
        label = hover and string.format(tr("verb.use_on"), item_name(held), name_of(hover)) or item_name(held)
    elseif hover ~= nil then
        label = string.format(tr("verb." .. (hover.verb == "exit" and "exit" or hover.verb)), name_of(hover))
    end
    ui.set_text(ids.HoverLabel, label)
    gamestate.set("hover", hover and hover.id or "")
    gamestate.set("cursor_verb", held and "item" or verb)
end

local function move_cursor(dt)
    local mx, my = scene.get_viewport_mouse()
    if last_mouse_x >= 0 and (math.abs(mx - last_mouse_x) > 0.5 or math.abs(my - last_mouse_y) > 0.5) then
        gamepad_mode = false
    end
    last_mouse_x, last_mouse_y = mx, my
    local dx, dy = input.get_action_vector("cursor")
    local vw, vh = scene.get_viewport_size()
    if math.abs(dx) + math.abs(dy) > 0.15 then
        gamepad_mode = true
        cursor_px = math.max(0, math.min(vw - 1, cursor_px + dx * vh * 0.9 * dt))
        cursor_py = math.max(0, math.min(vh - 1, cursor_py + dy * vh * 0.9 * dt))
    end
    local step = (input.is_action_just_pressed("next_hotspot") and 1) or
                 (input.is_action_just_pressed("prev_hotspot") and -1) or 0
    if step ~= 0 then
        gamepad_mode = true
        local visible = {}
        for _, hotspot in ipairs(hotspots) do
            if hotspot.enabled then visible[#visible + 1] = hotspot end
        end
        table.sort(visible, function(a, b)
            return transform.get_world_position(a.entity) < transform.get_world_position(b.entity)
        end)
        if #visible > 0 then
            local current = 0
            for index, hotspot in ipairs(visible) do
                if hover ~= nil and hotspot.id == hover.id then current = index end
            end
            local index = ((current - 1 + step) % #visible) + 1
            if current == 0 and step < 0 then index = #visible end
            local x, y = transform.get_world_position(visible[index].entity)
            cursor_px, cursor_py = to_pixel(x, y)
        end
    end
    if not gamepad_mode then
        cursor_px, cursor_py = mx, my
    end
end

-- -----------------------------------------------------------------------------------------------------------------
-- Acting.

local function face(x, y)
    local hx, hy = transform.get_world_position(ids.Hero)
    local dx, dy = x - hx, y - hy
    if math.abs(dx) < 0.05 and math.abs(dy) < 0.05 then return end
    -- Mostly sideways: a hotspot higher on the screen is behind, not straight up.
    transform.set_rotation(ids.Hero, 0, math.atan(dx, -dy * 0.35), 0)
end

-- Play the hero's "act" animation, then run `then_do` when it has reached out.
function A.act_animation(then_do, seconds)
    animator.trigger(ids.Hero, "act")
    busy_timer = seconds or 0.7
    after_busy = then_do
end

local function perform(action)
    local hotspot, verb, item = action.hotspot, action.verb, action.item
    local cx, cy = transform.get_world_position(hotspot.entity)
    face(cx, cy)
    if verb == "exit" and item == nil then
        A.go(hotspot.target)
        return
    end
    if room.act(hotspot.id, verb, item) then return end
    if verb == "look" then
        A.say("look." .. room.name .. "." .. hotspot.id)
    elseif item ~= nil then
        A.say("say.use_no", item_name(item))
    else
        A.say("say.no")
    end
end

-- The hero walks the corners of a path on the navigation mesh of the room (`RoomNav`, baked from its walk area when
-- the room starts), one leg after the other on its walk agent, which keeps the scale by depth and the masks; without
-- a mesh, or for a point the mesh does not reach, the walk agent finds the way alone.
local route = {}

local function walk_to(x, y)
    route = {}
    local nav_id = ids.RoomNav or 0
    if nav_id ~= 0 then
        local hx, hy = transform.get_world_position(ids.Hero)
        local points = nav.find_path(nav_id, hx, hy, 0, x, y, 0)
        if points ~= nil and #points >= 6 then
            for index = 4, #points - 2, 3 do
                route[#route + 1] = { points[index], points[index + 1] }
            end
            gamestate.set("nav_routes", gamestate.get("nav_routes", 0) + 1)
            local first = table.remove(route, 1)
            if walk.go_to(ids.Hero, first[1], first[2]) then
                return true
            end
            route = {}
        end
    end
    return walk.go_to(ids.Hero, x, y)
end

-- Whether the hero still walks: the next leg starts when the last one is done.
local function walking()
    if walk.is_moving(ids.Hero) then
        return true
    end
    while #route > 0 do
        local corner = table.remove(route, 1)
        gamestate.set("nav_legs", gamestate.get("nav_legs", 0) + 1)
        if walk.go_to(ids.Hero, corner[1], corner[2]) and walk.is_moving(ids.Hero) then
            return true
        end
    end
    return false
end

local function start_action(hotspot, verb)
    hush()
    local item = held
    if verb ~= "look" and item ~= nil then
        hold(nil)
    end
    if verb == "look" then item = nil end
    local tx, ty = transform.get_world_position(hotspot.stand ~= 0 and hotspot.stand or hotspot.entity)
    pending = { hotspot = hotspot, verb = (item ~= nil) and "use" or verb, item = item, x = tx, y = ty }
    if not walk_to(tx, ty) then
        perform(pending)
        pending = nil
    end
    gamestate.set("actions", gamestate.get("actions", 0) + 1)
end

local function click(look)
    if hover == nil then
        if look then return end
        if held ~= nil then
            hold(nil)
            return
        end
        local wx, wy = to_world(cursor_px, cursor_py)
        pending = nil
        walk_to(wx, wy)
        hush()
        return
    end
    start_action(hover, look and "look" or hover.verb)
end

-- -----------------------------------------------------------------------------------------------------------------
-- The frame.

local function read_input(dt)
    if input.is_action_just_pressed("pause") then
        if inventory_open then
            set_inventory(false)
        elseif held ~= nil and not paused then
            hold(nil)
        else
            set_paused(not paused)
        end
        return
    end
    if paused then return end
    if conversation ~= nil then
        if input.is_action_just_pressed("act") and ui.get_focus() == 0 then on_dialogue_next() end
        return
    end
    if input.is_action_just_pressed("inventory") then
        set_inventory(not inventory_open)
        return
    end
    if inventory_open then
        local count = #inventory()
        if count > 0 and (input.is_action_just_pressed("next_hotspot") or input.is_action_just_pressed("prev_hotspot")) then
            local step = input.is_action_just_pressed("next_hotspot") and 1 or -1
            inventory_focus = ((inventory_focus - 1 + step) % count) + 1
            ui.set_focus(ids["Slot" .. inventory_focus])
        end
        if input.is_action_just_pressed("secondary") then
            if held ~= nil then hold(nil) else set_inventory(false) end
            return
        end
        -- A click on the room closes the bag; on a hotspot it also does what the cursor shows (use the object in hand).
        if not (input.is_action_just_pressed("primary") and not ui.is_mouse_over_ui()) then return end
        set_inventory(false)
        gamepad_mode = false
        move_cursor(dt)
        if hotspot_at(to_world(cursor_px, cursor_py)) == nil then
            update_cursor(dt)
            return
        end
    end
    move_cursor(dt)
    -- The hotspot under the cursor where it is now, not where it was last frame.
    update_cursor(dt)
    if busy_timer > 0 then return end
    local over_ui = ui.is_mouse_over_ui() and not gamepad_mode
    if speech_timer > 0 and (input.is_action_just_pressed("primary") or input.is_action_just_pressed("act")) and hover == nil then
        if not over_ui then hush() end
    end
    if over_ui then return end
    if input.is_action_just_pressed("primary") or input.is_action_just_pressed("act") then
        click(false)
    elseif input.is_action_just_pressed("secondary") or input.is_action_just_pressed("look") then
        if held ~= nil and input.is_action_just_pressed("secondary") then
            hold(nil)
        else
            click(true)
        end
    end
end

---@param dt any
function A.update(dt)
    read_input(dt)
    walking()
    update_cursor(0)
    if speech_timer > 0 then
        speech_timer = speech_timer - dt
        if speech_timer <= 0 then hush() end
    end
    if busy_timer > 0 then
        busy_timer = busy_timer - dt
        if busy_timer <= 0 and after_busy ~= nil then
            local run = after_busy
            after_busy = nil
            run()
        end
    end
    if pending ~= nil and not walking() then
        local action = pending
        pending = nil
        local hx, hy = transform.get_world_position(ids.Hero)
        if math.abs(hx - action.x) + math.abs(hy - action.y) < 1.5 + STRIDE then
            perform(action)
        end
    end
    if room.autosave_timer ~= nil then
        room.autosave_timer = room.autosave_timer - dt
        if room.autosave_timer <= 0 then
            room.autosave_timer = nil
            save.save_game(1)
        end
    end
end

-- -----------------------------------------------------------------------------------------------------------------
-- Start of a room.

function A.start(config)
    room = config
    room.ortho = room.ortho or 9
    A.leaving = false
    for _, name in ipairs({ "Hero", "Floor", "MainCamera", "Cursor", "ItemCursor", "Pointer", "HoverLabel",
                            "SpeechPanel", "SpeechText", "InventoryPanel", "BagButton", "DialoguePanel",
                            "DialogueSpeaker", "DialogueText", "DialogueNext", "PauseWindow", "PauseResume",
                            "RoomNav" }) do
        ids[name] = ui.find(name)
        if ids[name] == 0 then ids[name] = scene.find_entity(name) end
    end
    for _, item in ipairs(ITEMS) do
        ids["Held_" .. item] = ui.find("Held_" .. item)
    end
    glow.entity = 0
    cursor_hidden = false
    route = {}
    if ids.RoomNav ~= 0 then
        nav.bake(ids.RoomNav)
    end
    for slot = 1, SLOTS do
        ids["Slot" .. slot] = ui.find("Slot" .. slot)
        ids["SlotIcon" .. slot] = ui.find("SlotIcon" .. slot)
    end
    for index = 1, 3 do
        ids["Answer" .. index] = ui.find("Answer" .. index)
        ids["Answer" .. index .. "Text"] = ui.find("Answer" .. index .. "Text")
    end
    hotspots = parse(config.hotspots)
    -- The hero enters by the way he came; a loaded save keeps where he stood.
    local entry = gamestate.get("entry", "")
    if entry ~= "" then
        local spot = scene.find_entity("Entry_" .. entry)
        if spot ~= 0 then
            local x, y = transform.get_world_position(spot)
            local _, _, z = transform.get_position(ids.Hero)
            transform.set_position(ids.Hero, x, y, z)
            face(0, y)
        end
        gamestate.set("entry", "")
    end
    gamestate.set("room", room.name)
    held = gamestate.get("held", "") ~= "" and gamestate.get("held", "") or nil
    ui.set_visible(ids.DialoguePanel, false)
    ui.set_visible(ids.InventoryPanel, false)
    ui.set_visible(ids.PauseWindow, false)
    hush()
    A.refresh_inventory()
    local vw, vh = scene.get_viewport_size()
    cursor_px, cursor_py = vw / 2, vh / 2
    room.autosave_timer = 0.3
    if not ui.is_transition_active() then ui.transition_play("fade_in", 0.5, 0.02, 0.02, 0.05) end
end

return A
