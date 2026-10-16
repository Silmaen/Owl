-- Isometric town: the "build road" tool, the funds and the HUD of scenes/iso_town.owl.
-- Demonstrates: the `isometric` bindings (pick_cell under the mouse, get_cell / set_cell on the road layer,
--               get_slope, get_height), an edit of the map at runtime (no undo), the mouse, the gamepad stick and the
--               keyboard through input actions (`build`, `bulldoze`, `town_tool`, `cursor`), a HUD of its own
--               (huds/iso_town.owlhud) with tool buttons, ui.is_mouse_over_ui, gamestate.
--
-- The road layer (layer 1 of the tilemap) holds road overlays: tile 3 runs along X, tile 11 along Y; a cell follows the
-- axis of its road neighbours. The ground layer (layer 0) tells where one may build: grass, sand, dirt and flowers
-- only, on a slope that is not steep, never on a pavement plot, water or rock. `town_money`, `town_roads` (roads built
-- by the player) and `town_cursor_x` / `town_cursor_y` mirror the state for the other scripts and the scenario test.

local GROUND_LAYER, ROAD_LAYER = 0, 1
local ROAD_X, ROAD_Y = 3, 11
local BUILDABLE = { [0] = true, [1] = true, [4] = true, [6] = true, [7] = true }
local SIZE = 24
local FIRST_STEEP_SLOPE = 15
local START_FUNDS, ROAD_COST, ROAD_REFUND = 1000, 10, 5
local TOAST_SECONDS = 2.0
local PAD_REPEAT = 0.16

local TOOL_ROAD, TOOL_BULLDOZE = 1, 2
local TOOL_NAMES = { [TOOL_ROAD] = "Road", [TOOL_BULLDOZE] = "Bulldoze" }

local map_id, marker_id = 0, 0
local funds_id, roads_id, toast_id = 0, 0, 0
local tool_ids = {}
local tool = TOOL_ROAD
local cursor_x, cursor_y = 12, 12
local cursor_valid = false
local pad_mode = false
local pad_timer = 0
local last_mouse_x, last_mouse_y = -1, -1
local last_cell = nil
local toast_left = 0
local shown_funds, shown_roads = nil, nil
local built = {}

local function money()
    return gamestate.get("town_money", START_FUNDS)
end

local function toast(text)
    ui.set_text(toast_id, text)
    ui.set_visible(toast_id, true)
    toast_left = TOAST_SECONDS
end

local function is_road(tile)
    return tile == ROAD_X or tile == ROAD_Y
end

local function road_at(x, y)
    if x < 0 or y < 0 or x >= SIZE or y >= SIZE then
        return false
    end
    local _, overlay = isometric.get_cell(map_id, ROAD_LAYER, x, y)
    return is_road(overlay)
end

-- A road cell runs along Y when only its Y neighbours are roads, along X otherwise.
local function orientation(x, y)
    local along_y = road_at(x, y - 1) or road_at(x, y + 1)
    local along_x = road_at(x - 1, y) or road_at(x + 1, y)
    if along_y and not along_x then
        return ROAD_Y
    end
    return ROAD_X
end

local function refresh_cell(x, y)
    if road_at(x, y) then
        isometric.set_cell(map_id, ROAD_LAYER, x, y, -1, orientation(x, y))
    end
end

local function refresh_around(x, y)
    refresh_cell(x, y)
    refresh_cell(x - 1, y)
    refresh_cell(x + 1, y)
    refresh_cell(x, y - 1)
    refresh_cell(x, y + 1)
end

-- Why a road cannot go on a cell, or nil when it can.
local function build_problem(x, y)
    local ground = isometric.get_cell(map_id, GROUND_LAYER, x, y)
    if not BUILDABLE[ground] then
        return "Cannot build here"
    end
    if isometric.get_slope(map_id, x, y) >= FIRST_STEEP_SLOPE then
        return "Too steep"
    end
    local _, overlay = isometric.get_cell(map_id, ROAD_LAYER, x, y)
    if overlay ~= -1 then
        return "Already built"
    end
    if money() < ROAD_COST then
        return "Not enough funds"
    end
    return nil
end

local function build_road(x, y)
    local problem = build_problem(x, y)
    if problem then
        toast(problem)
        return
    end
    isometric.set_cell(map_id, ROAD_LAYER, x, y, -1, ROAD_X)
    refresh_around(x, y)
    built[y * SIZE + x] = true
    gamestate.set("town_money", money() - ROAD_COST)
    gamestate.set("town_roads", gamestate.get("town_roads", 0) + 1)
    toast("Road built")
end

local function bulldoze(x, y)
    if not road_at(x, y) then
        return
    end
    isometric.set_cell(map_id, ROAD_LAYER, x, y, -1, -1)
    refresh_around(x, y)
    gamestate.set("town_money", money() + ROAD_REFUND)
    if built[y * SIZE + x] then
        built[y * SIZE + x] = nil
        gamestate.set("town_roads", gamestate.get("town_roads", 0) - 1)
    end
    toast("Road removed")
end

local function set_tool(value)
    tool = value
    gamestate.set("town_tool", value)
    for index, id in pairs(tool_ids) do
        if id ~= 0 then
            local name = TOOL_NAMES[index]
            ui.set_text(id, index == tool and ("[" .. name .. "]") or name)
        end
    end
end

-- Called by the tool buttons of the HUD: a click on a button never builds.
function on_tool_road()
    set_tool(TOOL_ROAD)
end

function on_tool_bulldoze()
    set_tool(TOOL_BULLDOZE)
end

local function clamp(value)
    return math.max(0, math.min(SIZE - 1, value))
end

local function sign_step(value)
    if value > 0.5 then
        return 1
    elseif value < -0.5 then
        return -1
    end
    return 0
end

-- The cell the player aims at: under the mouse, or moved by the `cursor` action (right stick, I J K L).
local function update_cursor(dt)
    local mouse_x, mouse_y = input.get_mouse_x(), input.get_mouse_y()
    if mouse_x ~= last_mouse_x or mouse_y ~= last_mouse_y then
        last_mouse_x, last_mouse_y = mouse_x, mouse_y
        pad_mode = false
    end
    local right, down = input.get_action_vector("cursor")
    if right ~= 0 or down ~= 0 then
        pad_timer = pad_timer - dt
        if math.abs(right) > 0.5 or math.abs(down) > 0.5 then
            pad_mode = true
        end
        if pad_mode and pad_timer <= 0 then
            -- Screen right is world (+x, -y), screen down is world (+x, +y).
            cursor_x = clamp(cursor_x + sign_step(right + down))
            cursor_y = clamp(cursor_y + sign_step(down - right))
            pad_timer = PAD_REPEAT
        end
    else
        pad_timer = 0
    end
    if pad_mode then
        cursor_valid = true
    elseif ui.is_mouse_over_ui() then
        cursor_valid = false
    else
        local x, y = isometric.pick_cell(map_id)
        cursor_valid = x ~= nil
        if cursor_valid then
            cursor_x, cursor_y = x, y
        end
    end
    gamestate.set("town_cursor_x", cursor_valid and cursor_x or -1)
    gamestate.set("town_cursor_y", cursor_valid and cursor_y or -1)
    ui.set_visible(marker_id, cursor_valid)
    if cursor_valid then
        -- The marker sprite stands on the front corner of its cell.
        local z = isometric.get_height(map_id, cursor_x + 0.5, cursor_y + 0.5)
        transform.set_position(marker_id, cursor_x + 1, cursor_y + 1, z)
    end
end

local function update_hud(dt)
    local funds, roads = money(), gamestate.get("town_roads", 0)
    if funds ~= shown_funds then
        shown_funds = funds
        ui.set_text(funds_id, "Funds: " .. funds)
    end
    if roads ~= shown_roads then
        shown_roads = roads
        ui.set_text(roads_id, "Roads built: " .. roads)
    end
    if toast_left > 0 then
        toast_left = toast_left - dt
        if toast_left <= 0 then
            ui.set_visible(toast_id, false)
        end
    end
end

function on_create()
    map_id = scene.find_entity("Ground")
    marker_id = scene.find_entity("Cursor")
    funds_id = ui.find("FundsText")
    roads_id = ui.find("RoadsText")
    toast_id = ui.find("ToastText")
    tool_ids[TOOL_ROAD] = ui.find("ToolRoad")
    tool_ids[TOOL_BULLDOZE] = ui.find("ToolBulldoze")
    gamestate.set("town_money", START_FUNDS)
    gamestate.set("town_roads", 0)
    gamestate.set("town_trips", 0)
    ui.set_visible(toast_id, false)
    set_tool(TOOL_ROAD)
    ui.transition_fade_in(0.4)
end

function on_update(dt)
    if ui.is_transition_active() then
        return
    end
    update_hud(dt)
    update_cursor(dt)
    if input.is_action_just_pressed("town_tool") then
        set_tool(tool == TOOL_ROAD and TOOL_BULLDOZE or TOOL_ROAD)
    end
    local building = input.is_action_pressed("build")
    local removing = input.is_action_pressed("bulldoze")
    if not cursor_valid or not (building or removing) then
        last_cell = nil
        return
    end
    -- One action per cell while a button is held: a drag lays a road.
    local cell = cursor_y * SIZE + cursor_x
    if cell == last_cell then
        return
    end
    last_cell = cell
    if removing or tool == TOOL_BULLDOZE then
        bulldoze(cursor_x, cursor_y)
    else
        build_road(cursor_x, cursor_y)
    end
end
