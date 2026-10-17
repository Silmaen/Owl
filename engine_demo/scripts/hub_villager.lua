-- Villager of the hub: walks from point to point of the town over a navigation grid of the ground tilemap.
-- Demonstrates: tile-graph pathfinding in `cells` mode on a 2D tilemap (`path.find`, `path.poll`, `path.get`,
--               `path.release`, `path.is_walkable`): the `walkable` layer of tilemaps/world_map.owltilemap (hidden,
--               one cell wherever the ground is free of houses, trees, fences, lava and water; the bridge counts) is
--               the graph, so the route goes round every collidable tile. The overlay of F3 draws the graph over the
--               isometric view only, so this route is not shown.
--
-- A cell (tx, ty) of the map has its centre at (x0 + tx + 0.5, y0 - ty - 0.5) in the world, the map being centred on
-- its entity. The script keeps `villager_trips` (points reached), `villager_route_cells` (length of the last route),
-- `villager_offgrid` (steps onto a cell outside the graph) and `villager_x` / `villager_y` for the scenario test.

local WAYPOINTS = { { 4, 8 }, { 26, 9 }, { 26, 16 }, { 18, 21 }, { 8, 17 } }
local SPEED = 1.9
local WAIT = 1.2
local MAP_W, MAP_H = 48, 24

local map_id = 0
local x0, y0 = 0, 0
local sprite_scale = 1.0
local current = 1
local request = 0
local route, index = nil, 1
local x, y = 0, 0
local wait_left = 0
local clock = 0

local function world(cx, cy)
    return x0 + cx + 0.5, y0 - cy - 0.5
end

local function cell_of(wx, wy)
    return math.floor(wx - x0), math.floor(y0 - wy)
end

local function ask()
    local cx, cy = cell_of(x, y)
    local goal = WAYPOINTS[current % #WAYPOINTS + 1]
    request = path.find(map_id, cx, cy, goal[1], goal[2])
end

function on_create()
    map_id = scene.find_entity("WorldTilemap")
    local mx, my = transform.get_position(map_id)
    x0, y0 = mx - MAP_W / 2, my + MAP_H / 2
    sprite_scale = transform.get_scale(entity_id)
    x, y = world(WAYPOINTS[1][1], WAYPOINTS[1][2])
    transform.set_position(entity_id, x, y, 0.5)
    for _, key in ipairs({ "villager_trips", "villager_route_cells", "villager_offgrid" }) do
        gamestate.set(key, 0)
    end
    wait_left = 0.3
end

local function take_route()
    local cells = path.get(map_id, request)
    path.release(map_id, request)
    request = 0
    if cells ~= nil and #cells >= 4 then
        route, index = cells, 2
        gamestate.set("villager_route_cells", #cells // 2)
    else
        wait_left = WAIT
    end
end

local function walk(dt)
    local cx, cy = route[index * 2 - 1], route[index * 2]
    if not path.is_walkable(map_id, cx, cy) then
        gamestate.set("villager_offgrid", gamestate.get("villager_offgrid", 0) + 1)
    end
    local tx, ty = world(cx, cy)
    local dx, dy = tx - x, ty - y
    local dist = math.sqrt(dx * dx + dy * dy)
    local step = SPEED * dt
    if dist <= step then
        x, y = tx, ty
        index = index + 1
        if index > #route // 2 then
            route = nil
            current = current % #WAYPOINTS + 1
            gamestate.set("villager_trips", gamestate.get("villager_trips", 0) + 1)
            wait_left = WAIT
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
        if wait_left <= 0 and route == nil then
            ask()
        end
    elseif request ~= 0 then
        local status = path.poll(map_id, request)
        if status == "found" then
            take_route()
        elseif status ~= "pending" then
            path.release(map_id, request)
            request = 0
            wait_left = WAIT
        end
    elseif route ~= nil then
        walk(dt)
        clock = clock + dt
    end
    -- A small hop while walking, still when waiting.
    local hop = route ~= nil and math.abs(math.sin(clock * 9)) * 0.06 or 0
    transform.set_position(entity_id, x, y + hop, 0.5)
    gamestate.set("villager_x", x)
    gamestate.set("villager_y", y)
end
