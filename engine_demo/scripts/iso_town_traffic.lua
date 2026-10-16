-- Isometric town traffic: vehicles driving to destinations over the road graph of scenes/iso_town.owl.
-- Demonstrates: tile-graph pathfinding (`path.find`, `path.poll`, `path.get`, `path.is_valid`, `path.get_version`,
--               `path.get_edge_cost`, the `navigation` rules of the Ground tilemap), vehicles that re-route when a road
--               is bulldozed or built, the terrain height under them (isometric.get_height), one sprite per axis
--               switched with ui.set_visible (the sheet holds a frame for X and one for Y), gamestate.
--
-- Each vehicle owns a list of stops (cells of the road) and drives to the next one by the cheapest route. A trip pays
-- `TRIP_PAY` into `town_money` and counts in `town_trips`; a route that a bulldozed road cut counts in `town_reroutes`.
-- A vehicle with no route waits and asks again when the graph changes.

local TRIP_PAY = 5
local LANE = 0.2
local RETRY_SECONDS = 0.5

-- One vehicle: its sprites along X and along Y, its speed (cells per second), where it starts and where it goes.
local FLEET = {
    { x_sprite = "CarX", y_sprite = "CarY", speed = 2.2, start = { 13, 8 },
      stops = { { 18, 8 }, { 11, 19 }, { 2, 12 }, { 18, 16 } } },
    { x_sprite = "TruckX", y_sprite = "TruckY", speed = 1.5, start = { 6, 12 },
      stops = { { 11, 19 }, { 18, 8 }, { 2, 12 } } },
    { x_sprite = "CarBX", y_sprite = "CarBY", speed = 1.9, start = { 18, 12 },
      stops = { { 2, 12 }, { 11, 19 }, { 6, 8 } } },
}

local map_id = 0

local function center(path, index)
    return path[index * 2 - 1] + 0.5, path[index * 2] + 0.5
end

-- Ask for a route from the cell the vehicle stands on (or leaves) to its stop.
local function ask(vehicle)
    local stop = vehicle.stops[vehicle.stop]
    vehicle.request = path.find(map_id, vehicle.cell_x, vehicle.cell_y, stop[1], stop[2])
    vehicle.version = path.get_version(map_id)
    vehicle.retry = RETRY_SECONDS
end

local function next_stop(vehicle)
    vehicle.stop = vehicle.stop % #vehicle.stops + 1
end

local function release(vehicle)
    if vehicle.request ~= 0 then
        path.release(map_id, vehicle.request)
        vehicle.request = 0
    end
end

function on_create()
    map_id = scene.find_entity("Ground")
    for _, vehicle in ipairs(FLEET) do
        vehicle.x_id = scene.find_entity(vehicle.x_sprite)
        vehicle.y_id = scene.find_entity(vehicle.y_sprite)
        vehicle.cell_x, vehicle.cell_y = vehicle.start[1], vehicle.start[2]
        vehicle.stop = 1
        vehicle.route = nil
        vehicle.request = 0
        vehicle.version = 0
        vehicle.retry = 0
        vehicle.heading_x, vehicle.heading_y = 1, 0
        ask(vehicle)
    end
    gamestate.set("town_trips", 0)
    gamestate.set("town_reroutes", 0)
end

local function place(vehicle, x, y)
    local body_x, body_y = x - vehicle.heading_y * LANE, y + vehicle.heading_x * LANE
    local z = isometric.get_height(map_id, body_x, body_y)
    -- The sprite anchor is the front corner of the vehicle's cell.
    transform.set_position(vehicle.x_id, body_x + 0.5, body_y + 0.5, z)
    transform.set_position(vehicle.y_id, body_x + 0.5, body_y + 0.5, z)
    local along_x = vehicle.heading_x ~= 0
    ui.set_visible(vehicle.x_id, along_x)
    ui.set_visible(vehicle.y_id, not along_x)
end

-- A route arrived: drive it from its first cell.
local function take_route(vehicle)
    local cells = path.get(map_id, vehicle.request)
    release(vehicle)
    if cells == nil then
        return
    end
    vehicle.route = cells
    vehicle.next = 2
    vehicle.progress = 0
end

local function poll(vehicle, dt)
    if vehicle.request ~= 0 then
        local status = path.poll(map_id, vehicle.request)
        if status == "found" then
            take_route(vehicle)
        elseif status ~= "pending" then
            release(vehicle)
            vehicle.retry = RETRY_SECONDS
        end
        return
    end
    if vehicle.route == nil then
        vehicle.retry = vehicle.retry - dt
        if vehicle.retry <= 0 or path.get_version(map_id) ~= vehicle.version then
            ask(vehicle)
        end
    end
end

-- A road of the route is gone: stop and look for another one from the cell the vehicle is leaving.
local function check_route(vehicle)
    local version = path.get_version(map_id)
    if vehicle.route == nil or version == vehicle.version then
        return
    end
    vehicle.version = version
    local route = vehicle.route
    local last = #route // 2
    for index = vehicle.next, last do
        local from_x, from_y = route[index * 2 - 3], route[index * 2 - 2]
        local to_x, to_y = route[index * 2 - 1], route[index * 2]
        if path.get_edge_cost(map_id, from_x, from_y, to_x, to_y) == nil then
            vehicle.route = nil
            vehicle.progress = 0
            gamestate.set("town_reroutes", gamestate.get("town_reroutes", 0) + 1)
            ask(vehicle)
            return
        end
    end
end

local function drive(vehicle, dt)
    local route = vehicle.route
    local last = #route // 2
    vehicle.progress = vehicle.progress + vehicle.speed * dt
    while vehicle.progress >= 1 and vehicle.next <= last do
        vehicle.progress = vehicle.progress - 1
        vehicle.cell_x, vehicle.cell_y = route[vehicle.next * 2 - 1], route[vehicle.next * 2]
        vehicle.next = vehicle.next + 1
    end
    if vehicle.next > last then
        vehicle.route = nil
        gamestate.set("town_trips", gamestate.get("town_trips", 0) + 1)
        gamestate.set("town_money", gamestate.get("town_money", 0) + TRIP_PAY)
        next_stop(vehicle)
        ask(vehicle)
        place(vehicle, vehicle.cell_x + 0.5, vehicle.cell_y + 0.5)
        return
    end
    local from_x, from_y = center(route, vehicle.next - 1)
    local to_x, to_y = center(route, vehicle.next)
    vehicle.heading_x, vehicle.heading_y = to_x - from_x, to_y - from_y
    place(vehicle, from_x + vehicle.heading_x * vehicle.progress, from_y + vehicle.heading_y * vehicle.progress)
end

function on_update(dt)
    for _, vehicle in ipairs(FLEET) do
        check_route(vehicle)
        poll(vehicle, dt)
        if vehicle.route ~= nil then
            drive(vehicle, dt)
        else
            place(vehicle, vehicle.cell_x + 0.5, vehicle.cell_y + 0.5)
        end
    end
end
