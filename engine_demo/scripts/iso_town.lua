-- Isometric town: the "build road" tool, the funds and the HUD of scenes/iso_town.owl (and of its compact variant).
-- Demonstrates: the `isometric` bindings (pick_cell under the mouse, get_cell / set_cell on the road layer,
--               get_slope, get_height), an edit of the map at runtime (no undo), the mouse, the gamepad stick and the
--               keyboard through input actions (`build`, `bulldoze`, `town_tool`, `cursor`), a HUD of its own
--               (huds/iso_town.owlhud) with tool buttons, ui.is_mouse_over_ui, gamestate, sound effects of the tool
--               (`sound.play`, then `sound.set_volume` scaled by the `volume_sfx` setting).
--
-- The road layer (layer 1 of the tilemap) holds road overlays, 16 shapes (tiles 3, 11 and 12 to 25): a cell is tiled from
-- the mask of its four road neighbours (straight along X or Y, corner, T, cross, dead end, alone), and building or
-- bulldozing re-tiles the cell and its four neighbours. The ground layer (layer 0) tells where one may build: grass, sand, dirt and flowers
-- only, on a slope that is not steep, never on a pavement plot, water or rock. `town_money`, `town_roads` (roads built
-- by the player), `town_cursor_x` / `town_cursor_y` and `town_cursor_tile` (the road tile under the cursor) mirror the state for the other scripts and the scenario test.

local GROUND_LAYER, ROAD_LAYER = 0, 1
properties = {
    { name = "map_size", type = "float", default = 24.0 },
    { name = "target_trips", type = "float", default = 20.0 },
}

local ROAD_X, ROAD_Y = 3, 11
-- Neighbour mask bits: north is -y, east +x, south +y, west -x.
local NORTH, EAST, SOUTH, WEST = 1, 2, 4, 8
-- Road tile of each neighbour mask, as laid out by tools/iso_atlas (their connections are in the tileset).
local TILE_OF_MASK = {
    [0] = 12,
    [NORTH] = 13, [EAST] = 14, [SOUTH] = 15, [WEST] = 16,
    [NORTH | EAST] = 17, [EAST | SOUTH] = 18, [SOUTH | WEST] = 19, [WEST | NORTH] = 20,
    [NORTH | EAST | SOUTH] = 21, [EAST | SOUTH | WEST] = 22, [SOUTH | WEST | NORTH] = 23, [WEST | NORTH | EAST] = 24,
    [NORTH | EAST | SOUTH | WEST] = 25,
    [EAST | WEST] = ROAD_X, [NORTH | SOUTH] = ROAD_Y,
}
local FIRST_ROAD_TILE, LAST_ROAD_TILE = 11, 25
local BUILDABLE = { [0] = true, [1] = true, [4] = true, [6] = true, [7] = true }
local SIZE = 24
local FIRST_STEEP_SLOPE = 15
local START_FUNDS, ROAD_COST, ROAD_REFUND = 1000, 10, 5
-- Victory objective: the town earns its keep once its vehicles have made this many trips (`town_trips`), set by the
-- scene (`target_trips`).
local TARGET_TRIPS = 20
local TOAST_SECONDS = 2.0
local PAD_REPEAT = 0.16

local TOOL_ROAD, TOOL_BULLDOZE = 1, 2
local TOOL_NAMES = { [TOOL_ROAD] = "Road", [TOOL_BULLDOZE] = "Bulldoze" }

local map_id, marker_id = 0, 0
local funds_id, roads_id, toast_id, objective_id = 0, 0, 0, 0
local leaving = false
local shown_trips = nil
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

-- A sound effect of the tool: `volume` times the sound effects volume of the settings. `town_sounds` counts them.
local function play(path, volume)
    gamestate.set("town_sounds", gamestate.get("town_sounds", 0) + 1)
    local handle = sound.play(path)
    sound.set_volume(handle, volume * settings.get("volume_sfx", 1.0))
end

local function money()
    return gamestate.get("town_money", START_FUNDS)
end

local function toast(text)
    ui.set_text(toast_id, text)
    ui.set_visible(toast_id, true)
    toast_left = TOAST_SECONDS
end

local function is_road(tile)
    return tile == ROAD_X or (tile >= FIRST_ROAD_TILE and tile <= LAST_ROAD_TILE)
end

local function road_at(x, y)
    if x < 0 or y < 0 or x >= SIZE or y >= SIZE then
        return false
    end
    local _, overlay = isometric.get_cell(map_id, ROAD_LAYER, x, y)
    return is_road(overlay)
end

-- The road tile of a cell from which of its four neighbours are roads.
local function tile_for(x, y)
    local mask = 0
    if road_at(x, y - 1) then mask = mask | NORTH end
    if road_at(x + 1, y) then mask = mask | EAST end
    if road_at(x, y + 1) then mask = mask | SOUTH end
    if road_at(x - 1, y) then mask = mask | WEST end
    return TILE_OF_MASK[mask]
end

local function refresh_cell(x, y)
    if road_at(x, y) then
        isometric.set_cell(map_id, ROAD_LAYER, x, y, -1, tile_for(x, y))
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
        play("sounds/town/error.wav", 0.5)
        return
    end
    isometric.set_cell(map_id, ROAD_LAYER, x, y, -1, ROAD_X) -- any road tile: refresh_around picks the shape
    refresh_around(x, y)
    built[y * SIZE + x] = true
    gamestate.set("town_money", money() - ROAD_COST)
    gamestate.set("town_roads", gamestate.get("town_roads", 0) + 1)
    toast("Road built")
    play("sounds/town/click_build.wav", 0.7)
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
    play("sounds/town/click_bulldoze.wav", 0.7)
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
    local under = -1
    if cursor_valid then
        under = select(2, isometric.get_cell(map_id, ROAD_LAYER, cursor_x, cursor_y))
    end
    gamestate.set("town_cursor_tile", under)
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
    local trips = gamestate.get("town_trips", 0)
    if trips ~= shown_trips then
        shown_trips = trips
        ui.set_text(objective_id, "Objective: " .. math.min(trips, TARGET_TRIPS) .. " / " .. TARGET_TRIPS ..
            " trips made by the vehicles")
    end
    if trips >= TARGET_TRIPS and not leaving then
        -- The goal is reached: the level is done, back to the hub.
        leaving = true
        gamestate.set("level_iso_done", 1)
        gamestate.remove("iso_return")
        play("sounds/victory.wav", 1.0)
        scene.transition_to("scenes/world_map.owl", "fade_out", 0.4)
    end
end

function on_create()
    SIZE = math.floor(map_size or SIZE)
    TARGET_TRIPS = math.floor(target_trips or TARGET_TRIPS)
    cursor_x, cursor_y = SIZE // 2, SIZE // 2
    map_id = scene.find_entity("Ground")
    marker_id = scene.find_entity("Cursor")
    funds_id = ui.find("FundsText")
    roads_id = ui.find("RoadsText")
    objective_id = ui.find("ObjectiveText")
    toast_id = ui.find("ToastText")
    tool_ids[TOOL_ROAD] = ui.find("ToolRoad")
    tool_ids[TOOL_BULLDOZE] = ui.find("ToolBulldoze")
    gamestate.set("town_money", START_FUNDS)
    gamestate.set("town_roads", 0)
    gamestate.set("town_trips", 0)
    gamestate.set("town_sounds", 0)
    ui.set_visible(toast_id, false)
    set_tool(TOOL_ROAD)
    ui.transition_fade_in(0.4)
end

function on_update(dt)
    if leaving or ui.is_transition_active() then
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
