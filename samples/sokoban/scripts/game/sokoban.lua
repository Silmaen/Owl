-- The rules of a Sokoban level, on the `Game` entity of each level scene (one scene per level, gap G14).
--
-- The level is the tilemap of the `Level` entity, read and written with `isometric.get_cell` / `isometric.set_cell`
-- (they work on any tilemap, gap G6; `isometric.get_size` gives its grid): layer 0 holds the floor and the walls, layer 1
-- the goals, layer 2 the crates and the hero tile where the player starts (cleared at start). A level is edited in the
-- tilemap editor of Owl Nest by painting those three layers; on_create checks that crates and goals match.
--
-- - One step per press of a `step_*` action (keys, D-pad) or a push of the left stick (`step_stick`), repeated while
--   held. The player and a pushed crate slide to their cell (`MovingBox` carries the crate during the slide).
-- - Every step goes on an undo stack (a Lua table): `undo` (Z, U, Backspace, pad B) takes back the last one, `restart`
--   (R, pad Y) takes them all back. Moves and pushes count in the HUD.
-- - Every crate on a goal: the level is done. `sokoban_done_NN` and `sokoban_best_NN` (fewest moves) are kept as
--   settings (saved at once), then the next level, or the victory screen after the last one.
-- - `pause` (Esc, P, Start) opens the pause window of `huds/level.owlhud`; the HUD buttons Undo / Restart / Menu do the
--   same with the mouse.
-- - The state is mirrored into `gamestate` for the scenarios: `sokoban_level`, `sokoban_moves`, `sokoban_pushes`,
--   `sokoban_boxes`, `sokoban_goals`, `sokoban_on_goal`, `sokoban_player_x` / `_y`, `sokoban_won`, `sokoban_done_NN`.

properties = {
    { name = "level", type = "int", default = 1 },
}

-- Written out so that the export packs every level: it follows the scene paths quoted in scripts.
local LEVEL_SCENES = {
    "scenes/level_01.owl",
    "scenes/level_02.owl",
    "scenes/level_03.owl",
    "scenes/level_04.owl",
    "scenes/level_05.owl",
    "scenes/level_06.owl",
    "scenes/level_07.owl",
    "scenes/level_08.owl",
    "scenes/level_09.owl",
    "scenes/level_10.owl",
}
local LEVEL_NAMES = { "First push", "Two by two", "Side door", "Back room", "Cellar", "Four corners", "Pantry",
                      "Columns", "Loading bay", "Warehouse" }

local FLOOR, GOALS, BOXES = 0, 1, 2
local EMPTY = -1
local TILE_FLOOR, TILE_FLOOR_WORN = 0, 1
local TILE_GOAL, TILE_BOX, TILE_BOX_ON_GOAL, TILE_START = 4, 5, 6, 7
local FRAME_IDLE, FRAME_WALK, WALK_FRAMES, FRAME_PUSH = 8, 9, 4, 13

local SLIDE_TIME = 0.08        -- seconds to slide one cell
local REPEAT_DELAY = 0.25      -- a held direction repeats after this
local REPEAT_TIME = 0.11       -- then one step every this
local STICK_ON, STICK_OFF = 0.6, 0.35
local WIN_DELAY = 2.2

local DIRECTIONS = {
    step_left = { -1, 0 },
    step_right = { 1, 0 },
    step_up = { 0, -1 },
    step_down = { 0, 1 },
}

local map_id, player_id, box_id = 0, 0, 0
local hud = {}
local width, height, cell_size = 0, 0, 1.0
local origin_x, origin_y = 0.0, 0.0
local px, py = 0, 0                -- player cell
local facing = 1                   -- 1 right, -1 left
local history = {}                 -- undo stack: { dx, dy, pushed }
local moves, pushes = 0, 0
local goal_count, on_goal = 0, 0
local slide = nil                  -- { clock, from_x, from_y, box = { from_x, from_y, cx, cy } or nil }
local held, held_clock, queued = nil, 0.0, nil
local stick_dir = nil
local walk_frame = 0
local paused = false
local won, win_clock = false, 0.0
local leaving = false
local message_clock = 0.0
local shown = {}

-- Grid ----------------------------------------------------------------------------------------------------------------

local function terrain(layer, x, y)
    if x < 0 or y < 0 or x >= width or y >= height then
        return EMPTY
    end
    return (isometric.get_cell(map_id, layer, x, y))
end

local function is_floor(x, y)
    local tile = terrain(FLOOR, x, y)
    return tile == TILE_FLOOR or tile == TILE_FLOOR_WORN
end

local function is_goal(x, y)
    return terrain(GOALS, x, y) == TILE_GOAL
end

local function is_box(x, y)
    local tile = terrain(BOXES, x, y)
    return tile == TILE_BOX or tile == TILE_BOX_ON_GOAL
end

local function put_box(x, y, present)
    local tile = EMPTY
    if present then
        tile = is_goal(x, y) and TILE_BOX_ON_GOAL or TILE_BOX
    end
    isometric.set_cell(map_id, BOXES, x, y, tile, EMPTY)
end

-- The grid is centred on the `Level` entity.
local function cell_to_world(x, y)
    return origin_x + (x - (width - 1) / 2) * cell_size, origin_y + ((height - 1) / 2 - y) * cell_size
end

local function count_goals()
    local count = 0
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            if is_goal(x, y) and is_box(x, y) then count = count + 1 end
        end
    end
    return count
end

-- HUD -----------------------------------------------------------------------------------------------------------------

local function set_text(key, text)
    if hud[key] ~= 0 and shown[key] ~= text then
        ui.set_text(hud[key], text)
        shown[key] = text
    end
end

local function show_message(text, seconds)
    if hud.message == 0 then return end
    set_text("message", text)
    ui.set_visible(hud.message, true)
    message_clock = seconds or 0.0
end

local function best_key()
    return string.format("sokoban_best_%02d", level)
end

local function done_key()
    return string.format("sokoban_done_%02d", level)
end

local function update_hud()
    set_text("level", "Level " .. level .. " / " .. #LEVEL_SCENES)
    set_text("moves", "Moves " .. moves .. "   Pushes " .. pushes)
    local best = settings.get(best_key(), 0)
    set_text("best", best > 0 and ("Best " .. best) or "Best -")
end

local function publish()
    gamestate.set("sokoban_moves", moves)
    gamestate.set("sokoban_pushes", pushes)
    gamestate.set("sokoban_on_goal", on_goal)
    gamestate.set("sokoban_player_x", px)
    gamestate.set("sokoban_player_y", py)
    gamestate.set("sokoban_won", won)
    update_hud()
end

-- Player --------------------------------------------------------------------------------------------------------------

local function place_player(frame)
    local wx, wy = cell_to_world(px, py)
    transform.set_position(player_id, wx, wy, 0.1)
    transform.set_scale(player_id, facing, 1, 1)
    isometric.set_sprite_frame(player_id, frame)
end

local function finish_slide()
    if slide == nil then return end
    if slide.box then
        put_box(slide.box.cx, slide.box.cy, true)
        ui.set_visible(box_id, false)
        if is_goal(slide.box.cx, slide.box.cy) then sound.play("sounds/game/goal.wav") end
    end
    slide = nil
    place_player(FRAME_IDLE)
    on_goal = count_goals()
    publish()
end

local function check_win()
    if won or goal_count == 0 or on_goal < goal_count then return end
    won = true
    win_clock = 0.0
    sound.play("sounds/game/level_clear.wav")
    local best = settings.get(best_key(), 0)
    local text = "Level complete!\n" .. moves .. " moves, " .. pushes .. " pushes"
    if best == 0 or moves < best then
        settings.set(best_key(), moves)
        if best > 0 then text = text .. "   New best!" end
    end
    settings.set(done_key(), true)
    settings.save()
    gamestate.set(done_key(), true)
    gamestate.set("sokoban_won", true)
    show_message(text)
    update_hud()
end

local function step(dx, dy)
    finish_slide()
    if won then return end
    if dx ~= 0 then facing = dx end
    local nx, ny = px + dx, py + dy
    if not is_floor(nx, ny) then
        place_player(FRAME_IDLE)
        sound.play("sounds/game/bump.wav")
        return
    end
    local box = nil
    if is_box(nx, ny) then
        local bx, by = nx + dx, ny + dy
        if not is_floor(bx, by) or is_box(bx, by) then
            place_player(FRAME_PUSH)
            sound.play("sounds/game/bump.wav")
            return
        end
        put_box(nx, ny, false)
        box = { from_x = nx, from_y = ny, cx = bx, cy = by }
        isometric.set_sprite_frame(box_id, is_goal(bx, by) and TILE_BOX_ON_GOAL or TILE_BOX)
        local wx, wy = cell_to_world(nx, ny)
        transform.set_position(box_id, wx, wy, 0.05)
        ui.set_visible(box_id, true)
        pushes = pushes + 1
        sound.play("sounds/game/push.wav")
    else
        sound.play("sounds/game/step.wav")
    end
    slide = { clock = 0.0, from_x = px, from_y = py, box = box }
    px, py = nx, ny
    moves = moves + 1
    history[#history + 1] = { dx, dy, box ~= nil }
    walk_frame = (walk_frame + 1) % WALK_FRAMES
    publish()
end

local function update_slide(dt)
    if slide == nil then return end
    slide.clock = slide.clock + dt
    local t = math.min(1.0, slide.clock / SLIDE_TIME)
    local fx, fy = cell_to_world(slide.from_x, slide.from_y)
    local tx, ty = cell_to_world(px, py)
    transform.set_position(player_id, fx + (tx - fx) * t, fy + (ty - fy) * t, 0.1)
    transform.set_scale(player_id, facing, 1, 1)
    isometric.set_sprite_frame(player_id, slide.box and FRAME_PUSH or FRAME_WALK + walk_frame)
    if slide.box then
        local bfx, bfy = cell_to_world(slide.box.from_x, slide.box.from_y)
        local btx, bty = cell_to_world(slide.box.cx, slide.box.cy)
        transform.set_position(box_id, bfx + (btx - bfx) * t, bfy + (bty - bfy) * t, 0.05)
    end
    if t >= 1.0 then
        finish_slide()
        check_win()
    end
end

-- Undo and restart ----------------------------------------------------------------------------------------------------

local function take_back()
    local last = table.remove(history)
    if last == nil then return false end
    local dx, dy, pushed = last[1], last[2], last[3]
    if pushed then
        put_box(px + dx, py + dy, false)
        put_box(px, py, true)
        pushes = pushes - 1
    end
    px, py = px - dx, py - dy
    moves = moves - 1
    return true
end

local function undo()
    finish_slide()
    if won then return end
    if take_back() then
        sound.play("sounds/game/undo.wav")
        place_player(FRAME_IDLE)
        on_goal = count_goals()
        publish()
    end
end

local function restart()
    finish_slide()
    if won or #history == 0 then return end
    while take_back() do end
    moves, pushes = 0, 0
    sound.play("sounds/game/restart.wav")
    place_player(FRAME_IDLE)
    on_goal = count_goals()
    publish()
    show_message("Restarted", 0.8)
end

-- Pause and buttons ---------------------------------------------------------------------------------------------------

local function set_paused(value)
    paused = value
    gamestate.set("sokoban_paused", value)
    if hud.pause ~= 0 then ui.set_visible(hud.pause, value) end
    if value and hud.resume ~= 0 then ui.set_focus(hud.resume) end
    held, queued = nil, nil
end

local function leave(path)
    if leaving then return end
    leaving = true
    scene.transition_to(path, "fade_out", 0.3)
end

function on_pause_resume() set_paused(false) end

function on_pause_restart()
    set_paused(false)
    restart()
end

function on_pause_levels()
    set_paused(false)
    leave("scenes/level_select.owl")
end

function on_pause_quit()
    set_paused(false)
    leave("scenes/title.owl")
end

function on_undo_clicked()
    if not paused then undo() end
end

function on_restart_clicked()
    if not paused then restart() end
end

function on_menu_clicked()
    if not won then set_paused(not paused) end
end

-- Lifecycle -----------------------------------------------------------------------------------------------------------

function on_create()
    ui.transition_fade_in(0.4)
    map_id = scene.find_entity("Level")
    player_id = scene.find_entity("Player")
    box_id = scene.find_entity("MovingBox")
    hud.level = ui.find("LevelText")
    hud.moves = ui.find("MovesText")
    hud.best = ui.find("BestText")
    hud.message = ui.find("MessageText")
    hud.pause = ui.find("PauseWindow")
    hud.resume = ui.find("ResumeButton")
    if hud.pause ~= 0 then ui.set_visible(hud.pause, false) end
    if box_id ~= 0 then ui.set_visible(box_id, false) end
    origin_x, origin_y = transform.get_position(map_id)
    width, height, cell_size = isometric.get_size(map_id)
    width, height, cell_size = width or 0, height or 0, cell_size or 1.0

    local boxes, start_found = 0, false
    goal_count = 0
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            if terrain(BOXES, x, y) == TILE_START then
                isometric.set_cell(map_id, BOXES, x, y, EMPTY, EMPTY)
                px, py, start_found = x, y, true
            end
            if is_box(x, y) then
                boxes = boxes + 1
                put_box(x, y, true)
            end
            if is_goal(x, y) then goal_count = goal_count + 1 end
        end
    end
    if not start_found then
        log.warn("Sokoban: level " .. level .. " has no start tile in layer 2, the player starts at the top left.")
    end
    if boxes ~= goal_count then
        log.warn("Sokoban: level " .. level .. " has " .. boxes .. " crates for " .. goal_count .. " goals.")
    end
    on_goal = count_goals()
    place_player(FRAME_IDLE)

    gamestate.set("sokoban_level", level)
    gamestate.set("sokoban_boxes", boxes)
    gamestate.set("sokoban_goals", goal_count)
    gamestate.set("sokoban_paused", false)
    gamestate.set(done_key(), settings.get(done_key(), false))
    publish()
    show_message("Level " .. level .. ": " .. (LEVEL_NAMES[level] or ""), 1.8)
end

local function read_direction()
    for action, dir in pairs(DIRECTIONS) do
        if input.is_action_just_pressed(action) then
            return action, dir
        end
    end
    return nil, nil
end

-- The left stick as four buttons: a direction past STICK_ON presses, back under STICK_OFF releases.
local function read_stick()
    local sx, sy = input.get_action_vector("step_stick")
    local magnitude = math.max(math.abs(sx), math.abs(sy))
    if stick_dir ~= nil and magnitude < STICK_OFF then
        stick_dir = nil
    elseif stick_dir == nil and magnitude > STICK_ON then
        if math.abs(sx) > math.abs(sy) then
            stick_dir = sx > 0 and "step_right" or "step_left"
        else
            stick_dir = sy > 0 and "step_down" or "step_up"
        end
        return stick_dir
    end
    return nil
end

local function is_held(action)
    return (action ~= nil and input.is_action_pressed(action)) or (action ~= nil and action == stick_dir)
end

local function update_input(dt)
    local stick = read_stick()
    local action = read_direction() or stick
    if action ~= nil then
        held, held_clock = action, 0.0
        if slide ~= nil then
            queued = action
        else
            step(DIRECTIONS[action][1], DIRECTIONS[action][2])
        end
    elseif is_held(held) then
        held_clock = held_clock + dt
        if held_clock >= REPEAT_DELAY and slide == nil then
            held_clock = REPEAT_DELAY - REPEAT_TIME
            step(DIRECTIONS[held][1], DIRECTIONS[held][2])
        end
    else
        held = nil
    end
    if queued ~= nil and slide == nil then
        local dir = DIRECTIONS[queued]
        queued = nil
        step(dir[1], dir[2])
    end
    if input.is_action_just_pressed("undo") then undo() end
    if input.is_action_just_pressed("restart") then restart() end
end

function on_update(dt)
    if leaving then return end
    if not won and input.is_action_just_pressed("pause") then
        set_paused(not paused)
    end
    if message_clock > 0.0 then
        message_clock = message_clock - dt
        if message_clock <= 0.0 then ui.set_visible(hud.message, false) end
    end
    if paused then return end
    update_slide(dt)
    if won then
        win_clock = win_clock + dt
        if win_clock >= WIN_DELAY then
            leave(LEVEL_SCENES[level + 1] or "scenes/victory.owl")
        end
        return
    end
    update_input(dt)
end
