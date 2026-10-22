-- Villager of the hub: walks from point to point of the village over a navigation grid of the ground tilemap.
-- Demonstrates: tile-graph pathfinding in `cells` mode on a 2D tilemap (`path.find`, `path.poll`, `path.get`,
--               `path.release`, `path.is_walkable`): the `walkable` layer of tilemaps/world_map.owltilemap (hidden,
--               one cell wherever the ground is free of houses, trees, fences, stalls, lava and water; the bridges count) is
--               the graph, so the route goes round every collidable tile. The overlay of F3 draws the graph over the
--               isometric view only, so this route is not shown. Two entities share this script: the `route` property
--               ("column,row;column,row;...") and the `key` of their counters in gamestate tell them apart.
--
-- A cell (tx, ty) of the map has its centre at (x0 + tx + 0.5, y0 - ty - 0.5) in the world, the map being centred on
-- its entity. The script keeps `<key>_trips` (points reached), `<key>_route_cells` (length of the last route),
-- `<key>_offgrid` (steps onto a cell outside the graph) and `<key>_x` / `<key>_y` for the scenario test. The character
-- walks on two frames and breathes on two others (`idle_a`, `idle_b`, `walk_a`, `walk_b`: tiles of the town atlas).

properties = {
    { name = "route", type = "string", default = "" },
    { name = "key", type = "string", default = "villager" },
    { name = "speed", type = "float", default = 1.9 },
    { name = "idle_a", type = "float", default = 0 },
    { name = "idle_b", type = "float", default = 0 },
    { name = "walk_a", type = "float", default = 0 },
    { name = "walk_b", type = "float", default = 0 },
    { name = "wait_time", type = "float", default = 1.2, min = 0 },
    { name = "step_time", type = "float", default = 0.18, min = 0.01 },
}

local MAP_W, MAP_H = 64, 40

local waypoints = {}
local map_id = 0
local x0, y0 = 0, 0
local sprite_scale = 1.0
local current = 1
local request = 0
local path_cells, index = nil, 1
local x, y = 0, 0
local wait_left = 0
local clock = 0
local idle_clock = 0
local shown = -1

local function world(cx, cy)
    return x0 + cx + 0.5, y0 - cy - 0.5
end

local function cell_of(wx, wy)
    return math.floor(wx - x0), math.floor(y0 - wy)
end

local function show(frame)
    if frame ~= shown then
        shown = frame
        isometric.set_sprite_frame(entity_id, frame)
    end
end

local function ask()
    local cx, cy = cell_of(x, y)
    local goal = waypoints[current % #waypoints + 1]
    request = path.find(map_id, cx, cy, goal[1], goal[2])
end

function on_create()
    for cx, cy in string.gmatch(route, "(%d+),(%d+)") do
        waypoints[#waypoints + 1] = { tonumber(cx), tonumber(cy) }
    end
    map_id = scene.find_entity("WorldTilemap")
    local mx, my = transform.get_position(map_id)
    x0, y0 = mx - MAP_W / 2, my + MAP_H / 2
    sprite_scale = transform.get_scale(entity_id)
    x, y = world(waypoints[1][1], waypoints[1][2])
    transform.set_position(entity_id, x, y, 0.5)
    for _, suffix in ipairs({ "_trips", "_route_cells", "_offgrid" }) do
        gamestate.set(key .. suffix, 0)
    end
    wait_left = 0.3
end

local function take_route()
    local cells = path.get(map_id, request)
    path.release(map_id, request)
    request = 0
    if cells ~= nil and #cells >= 4 then
        path_cells, index = cells, 2
        gamestate.set(key .. "_route_cells", #cells // 2)
    else
        wait_left = wait_time
    end
end

local function walk(dt)
    local cx, cy = path_cells[index * 2 - 1], path_cells[index * 2]
    if not path.is_walkable(map_id, cx, cy) then
        gamestate.set(key .. "_offgrid", gamestate.get(key .. "_offgrid", 0) + 1)
    end
    local tx, ty = world(cx, cy)
    local dx, dy = tx - x, ty - y
    local dist = math.sqrt(dx * dx + dy * dy)
    local step = speed * dt
    if dist <= step then
        x, y = tx, ty
        index = index + 1
        if index > #path_cells // 2 then
            path_cells = nil
            current = current % #waypoints + 1
            gamestate.set(key .. "_trips", gamestate.get(key .. "_trips", 0) + 1)
            wait_left = wait_time
        end
    else
        x, y = x + dx / dist * step, y + dy / dist * step
        if dx ~= 0 then
            transform.set_scale(entity_id, dx < 0 and -sprite_scale or sprite_scale, sprite_scale, 1)
        end
    end
end

function on_update(dt)
    if wait_left > 0 then
        wait_left = wait_left - dt
        if wait_left <= 0 and path_cells == nil then
            ask()
        end
    elseif request ~= 0 then
        local status = path.poll(map_id, request)
        if status == "found" then
            take_route()
        elseif status ~= "pending" then
            path.release(map_id, request)
            request = 0
            wait_left = wait_time
        end
    elseif path_cells ~= nil then
        walk(dt)
        clock = clock + dt
    end
    local walking = path_cells ~= nil
    if walking then
        show(math.floor(clock / step_time) % 2 == 0 and walk_a or walk_b)
    else
        idle_clock = idle_clock + dt
        show(math.floor(idle_clock / 0.4) % 2 == 0 and idle_a or idle_b)
    end
    -- A small hop while walking, still when waiting.
    local hop = walking and math.abs(math.sin(clock * 9)) * 0.06 or 0
    transform.set_position(entity_id, x, y + hop, 0.5)
    gamestate.set(key .. "_x", x)
    gamestate.set(key .. "_y", y)
end
