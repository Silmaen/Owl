-- Isometric town traffic: vehicles driving to destinations over the road graph of scenes/iso_town.owl.
-- Demonstrates: tile-graph pathfinding (`path.find`, `path.poll`, `path.get`, `path.is_valid`, `path.get_version`,
--               `path.get_edge_cost`, `path.is_walkable`, the tileset connections of the Ground tilemap), the debug
--               overlay (`path.set_debug`, `path.show_route`: F3 draws the road graph and the route of each vehicle in
--               its own colour, in the game and in the Play view of Owl Nest), vehicles that
--               re-route when a road is bulldozed or built, the terrain height under them (isometric.get_height), one
--               sprite per vehicle whose frame follows its heading every 45 degrees (isometric.set_sprite_frame),
--               gamestate.
--
-- Each vehicle drives to a destination picked at random among the cells of the road (a generator seeded per vehicle, so a
-- run is repeatable), by the cheapest route that never turns back (`no_reverse` and the heading of the vehicle in
-- `path.find`): a vehicle goes on the way it was heading and reverses only at a dead end, a cell with a single exit. On
-- arrival it picks another destination. A trip pays `TRIP_PAY` into `town_money` and counts in `town_trips`; a route that a
-- bulldozed road cut counts in `town_reroutes`. A vehicle with no route waits and asks again when the graph changes.
-- Every vehicle plans its route again at its next cell boundary when the graph changed (`path.get_version`), so a new
-- road is taken if shorter: `town_replans`. A vehicle left standing on a bulldozed cell asks from the road cell it came
-- from, without the heading, so it drives off and finds its way again. The route of each vehicle is given to the overlay
-- at every plan (`town_route_calls` counts the `path.show_route` calls).
--
-- Traffic keeps to the right as seen on screen. World +x runs to the lower right and +y to the lower left, so the right
-- hand of a vehicle heading (dx, dy) is (-dy, dx): driving to the lower right it keeps to the lower left side of the
-- road. A vehicle crosses each cell of its route from the middle of the edge it enters by to the middle of the edge it
-- leaves by, LANE away from the middle of the road; in a corner it follows a quarter circle around the corner of the
-- cell, its heading turning with it; at a dead end it blends from its lane to a circle round the middle of the cell,
-- drives half of it turning left, through the far half of the cell, and comes back out by the other lane. The sheet holds eight frames per model, every 45 degrees from +x
-- towards +y.
--
-- The script counts what the scenario `iso_town.owltest` checks: `town_offroad` (updates a moving vehicle stood off the
-- road), `town_frame_errors` (frame not matching the heading), `town_heading_errors` (moving away from its heading) and
-- `town_shortcuts` (re-plans that found a shorter route, a road built or rebuilt), `town_bad_uturns` (reversals outside a
-- dead end), `town_dead_end_loops` (loops at a dead end), `town_frames_seen` (number of distinct headings shown, 0 to 8), `town_longest_wait` (the longest time, in seconds, a
-- vehicle waited without a route) and `town_debug` (1 while the navigation overlay is on).

local TRIP_PAY = 5
local LANE = 0.2
local RETRY_SECONDS = 0.5
local HEADINGS = 8
local TURN = math.pi / 2
local HEADING_STEP = math.pi / 4

-- One vehicle: its sprite, the first frame of its model in the sheet (8 frames per model), its speed (cells per
-- second), where it starts, its colour in the overlay and the seed of its destinations.
local FLEET = {
    { sprite = "Car", model = 0, speed = 2.2, start = { 13, 8 }, color = { 1.0, 0.35, 0.3 }, seed = 11 },
    { sprite = "Truck", model = 8, speed = 1.5, start = { 6, 12 }, color = { 0.3, 0.6, 1.0 }, seed = 23 },
    { sprite = "CarB", model = 0, speed = 1.9, start = { 18, 12 }, color = { 1.0, 0.9, 0.2 }, seed = 37 },
}

local SIZE = 24
local MIN_TRIP = 4
local LOOP = 0.36

local map_id = 0

local function center(path, index)
    return path[index * 2 - 1] + 0.5, path[index * 2] + 0.5
end

local NEIGHBOURS = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } }

-- A vehicle whose cell was bulldozed under it stands off the graph: the route starts from the road cell it came from, or
-- failing that from a neighbouring one.
local function anchor(vehicle)
    if path.is_walkable(map_id, vehicle.cell_x, vehicle.cell_y) then
        vehicle.stranded = false
        return
    end
    local candidates = { { vehicle.prev_x, vehicle.prev_y } }
    for _, step in ipairs(NEIGHBOURS) do
        candidates[#candidates + 1] = { vehicle.cell_x + step[1], vehicle.cell_y + step[2] }
    end
    for _, cell in ipairs(candidates) do
        if cell[1] ~= nil and path.is_walkable(map_id, cell[1], cell[2]) then
            vehicle.cell_x, vehicle.cell_y = cell[1], cell[2]
            vehicle.stranded = true
            vehicle.dir_x, vehicle.dir_y = nil, nil
            return
        end
    end
end

-- Next number of the vehicle's generator (a linear congruential one: the same sequence on every run).
local function roll(vehicle, count)
    vehicle.rng = (vehicle.rng * 1103515245 + 12345) % 2147483648
    return (vehicle.rng // 65536) % count
end

-- Number of ways out of a cell of the road: one makes a dead end.
local function exits(x, y)
    local count = 0
    for _, step in ipairs(NEIGHBOURS) do
        if path.get_edge_cost(map_id, x, y, x + step[1], y + step[2]) ~= nil then
            count = count + 1
        end
    end
    return count
end

-- A road cell picked at random, a few cells away from the vehicle (any cell of the road when the road is too small).
local function choose_destination(vehicle)
    local fallback = nil
    for _ = 1, 400 do
        local x, y = roll(vehicle, SIZE), roll(vehicle, SIZE)
        if path.is_walkable(map_id, x, y) and (x ~= vehicle.cell_x or y ~= vehicle.cell_y) then
            if math.abs(x - vehicle.cell_x) + math.abs(y - vehicle.cell_y) >= MIN_TRIP then
                vehicle.dest = { x, y }
                return
            end
            fallback = fallback or { x, y }
        end
    end
    vehicle.dest = fallback
end

-- Ask for a route from the cell the vehicle stands on (or leaves) to its destination, going on the way it heads.
local function ask(vehicle)
    anchor(vehicle)
    if vehicle.dest == nil then
        choose_destination(vehicle)
    end
    vehicle.version = path.get_version(map_id)
    vehicle.retry = RETRY_SECONDS
    if vehicle.dest == nil then
        vehicle.request = 0
        return
    end
    if vehicle.stranded or vehicle.dir_x == nil then
        vehicle.request = path.find(map_id, vehicle.cell_x, vehicle.cell_y, vehicle.dest[1], vehicle.dest[2])
    else
        vehicle.request = path.find(map_id, vehicle.cell_x, vehicle.cell_y, vehicle.dest[1], vehicle.dest[2],
            { no_reverse = true, heading_x = vehicle.dir_x, heading_y = vehicle.dir_y })
    end
end

local function release(vehicle)
    if vehicle.request ~= 0 then
        path.release(map_id, vehicle.request)
        vehicle.request = 0
    end
end

local seen = {}

-- Frame of a model for a heading angle (radians, from +x towards +y): the nearest multiple of 45 degrees.
local function frame_of(vehicle, angle)
    return vehicle.model + math.floor(angle / HEADING_STEP + 0.5) % HEADINGS
end

local function wrap(angle)
    return (angle + math.pi) % (2 * math.pi) - math.pi
end

-- Give the debug overlay the rest of a vehicle's route, in the colour of the vehicle (nothing without a route). Always
-- given, shown or not: the overlay holds the current route of every vehicle when it is switched on.
local function publish(vehicle)
    if vehicle.route == nil then
        path.show_route(map_id, vehicle.id)
        return
    end
    local first = (vehicle.index - 1) * 2 + 1
    path.show_route(map_id, vehicle.id, table.move(vehicle.route, first, #vehicle.route, 1, {}), table.unpack(vehicle.color))
    gamestate.set("town_route_calls", gamestate.get("town_route_calls", 0) + 1)
end

function on_create()
    map_id = scene.find_entity("Ground")
    for _, vehicle in ipairs(FLEET) do
        vehicle.id = scene.find_entity(vehicle.sprite)
        vehicle.cell_x, vehicle.cell_y = vehicle.start[1], vehicle.start[2]
        vehicle.x, vehicle.y = vehicle.cell_x + 0.5, vehicle.cell_y + 0.5
        vehicle.angle = 0
        vehicle.rng = vehicle.seed
        vehicle.dest = nil
        vehicle.dir_x, vehicle.dir_y = nil, nil
        vehicle.route = nil
        vehicle.request = 0
        vehicle.version = 0
        vehicle.checked = 0
        vehicle.retry = 0
        vehicle.prev_x, vehicle.prev_y = nil, nil
        vehicle.stranded = false
        ask(vehicle)
    end
    gamestate.set("town_trips", 0)
    gamestate.set("town_reroutes", 0)
    gamestate.set("town_replans", 0)
    gamestate.set("town_shortcuts", 0)
    gamestate.set("town_offroad", 0)
    gamestate.set("town_frame_errors", 0)
    gamestate.set("town_heading_errors", 0)
    gamestate.set("town_frames_seen", 0)
    gamestate.set("town_longest_wait", 0)
    gamestate.set("town_debug", 0)
    gamestate.set("town_bad_uturns", 0)
    gamestate.set("town_dead_end_loops", 0)
    gamestate.set("town_route_calls", 0)
end

local function count(key)
    gamestate.set(key, gamestate.get(key, 0) + 1)
end

-- Show the vehicle at a ground position, its sprite frame following its heading angle.
local function place(vehicle, x, y, angle)
    vehicle.x, vehicle.y, vehicle.angle = x, y, angle
    local frame = frame_of(vehicle, angle)
    isometric.set_sprite_frame(vehicle.id, frame)
    -- The sprite is drawn centred on its tile, and its entity sits on the front corner of that tile.
    transform.set_position(vehicle.id, x + 0.5, y + 0.5, isometric.get_height(map_id, x, y))
    local heading = frame % HEADINGS
    if not seen[heading] then
        seen[heading] = true
        local distinct = 0
        for _ in pairs(seen) do
            distinct = distinct + 1
        end
        gamestate.set("town_frames_seen", distinct)
    end
end

-- Direction between two consecutive cells of the route, nil beyond its ends.
local function direction(route, index)
    if index < 1 or index >= #route // 2 then
        return nil
    end
    return route[index * 2 + 1] - route[index * 2 - 1], route[index * 2 + 2] - route[index * 2]
end

-- The U-turn of a dead end, as a rounded square that runs along the edges of the cell: in by the right lane, out to the
-- right side, round the two far corners turning left, back down the other side and in to the right lane of the way
-- out, so it passes by the four corners of the cell. Sampled once into a polyline; the heading is its tangent.
local LOOP_CORNER = 0.16
local LOOP_SAMPLES = 12

local function smooth(u)
    return u * u * (3 - 2 * u)
end

local function loop_path(cx, cy, d_x, d_y, right_x, right_y)
    local half, corner = LOOP, LOOP_CORNER
    local points = {}
    local function add(side, along)
        points[#points + 1] = { cx + right_x * side + d_x * along, cy + right_y * side + d_y * along }
    end
    -- The drift from the lane to the edge spans the whole straight, so the heading stays under 22.5 degrees and the
    -- sprite keeps its straight frame.
    local reach = half - corner + 0.5
    for i = 0, 2 * LOOP_SAMPLES do
        local u = i / (2 * LOOP_SAMPLES)
        add(LANE + (half - LANE) * smooth(u), -0.5 + reach * u)
    end
    for _, centre_side in ipairs({ half - corner, -(half - corner) }) do
        local start = centre_side > 0 and 0 or math.pi / 2
        for i = 1, LOOP_SAMPLES do
            local phi = start + (math.pi / 2) * i / LOOP_SAMPLES
            add(centre_side + corner * math.cos(phi), half - corner + corner * math.sin(phi))
        end
        if centre_side > 0 then
            for i = 1, LOOP_SAMPLES do
                add((half - corner) * (1 - 2 * i / LOOP_SAMPLES), half)
            end
        end
    end
    for i = 1, 2 * LOOP_SAMPLES do
        local u = i / (2 * LOOP_SAMPLES)
        add(-half + (half - LANE) * smooth(u), half - corner - reach * u)
    end
    local lengths, total = { 0 }, 0
    for i = 2, #points do
        total = total + math.sqrt((points[i][1] - points[i - 1][1]) ^ 2 + (points[i][2] - points[i - 1][2]) ^ 2)
        lengths[i] = total
    end
    return total, function(t)
        local target = math.max(0, math.min(1, t)) * total
        local i = 2
        while i < #points and lengths[i] < target do
            i = i + 1
        end
        local a, b = points[i - 1], points[i]
        local span = lengths[i] - lengths[i - 1]
        local f = span > 0 and (target - lengths[i - 1]) / span or 0
        return a[1] + (b[1] - a[1]) * f, a[2] + (b[2] - a[2]) * f, math.atan(b[2] - a[2], b[1] - a[1])
    end
end

-- The path through the cell `index` of the route: its length and a function giving the position and the heading angle
-- at a fraction of it. `entry_x`, `entry_y` is the direction the vehicle came into the first cell by, when it did.
local function cell_path(route, index, entry_x, entry_y)
    local cx, cy = route[index * 2 - 1] + 0.5, route[index * 2] + 0.5
    local in_x, in_y = entry_x, entry_y
    if index > 1 then
        in_x, in_y = direction(route, index - 1)
    end
    local out_x, out_y = direction(route, index)
    if in_x == nil and out_x == nil then
        return 0, function() return cx, cy, nil end
    end
    local d_in_x, d_in_y = in_x or out_x, in_y or out_y
    local d_out_x, d_out_y = out_x or in_x, out_y or in_y
    local cross = d_in_x * d_out_y - d_in_y * d_out_x
    if cross ~= 0 and in_x ~= nil and out_x ~= nil then
        -- A corner: a quarter circle around the corner of the cell inside the turn, a bit tighter for a turn to the right.
        local pivot_x = cx + (d_out_x - d_in_x) * 0.5
        local pivot_y = cy + (d_out_y - d_in_y) * 0.5
        local radius = 0.5 - cross * LANE
        local start_angle = math.atan(d_in_y, d_in_x)
        return radius * TURN, function(t)
            local phi = t * TURN
            return pivot_x + radius * (-d_out_x * math.cos(phi) + d_in_x * math.sin(phi)),
                   pivot_y + radius * (-d_out_y * math.cos(phi) + d_in_y * math.sin(phi)),
                   start_angle + cross * phi
        end
    end
    if cross == 0 and in_x ~= nil and out_x ~= nil and in_x * out_x + in_y * out_y < 0 then
        -- A U-turn at a dead end: a rounded square along the edges of the cell (`loop_path`). A U-turn anywhere else
        -- is counted as a mistake.
        if exits(route[index * 2 - 1], route[index * 2]) > 1 then
            count("town_bad_uturns")
        else
            count("town_dead_end_loops")
        end
        local right_x, right_y = -d_in_y, d_in_x
        return loop_path(cx, cy, d_in_x, d_in_y, right_x, right_y)
    end
    -- A straight run, or half of one in the first and the last cell of the route.
    local right_x, right_y = -d_out_y * LANE, d_out_x * LANE
    local from_x = (in_x ~= nil) and (cx - d_in_x * 0.5 + right_x) or (cx + right_x)
    local from_y = (in_y ~= nil) and (cy - d_in_y * 0.5 + right_y) or (cy + right_y)
    local to_x = (out_x ~= nil) and (cx + d_out_x * 0.5 + right_x) or (cx + right_x)
    local to_y = (out_y ~= nil) and (cy + d_out_y * 0.5 + right_y) or (cy + right_y)
    local angle = math.atan(d_out_y, d_out_x)
    return math.sqrt((to_x - from_x) ^ 2 + (to_y - from_y) ^ 2), function(t)
        return from_x + (to_x - from_x) * t, from_y + (to_y - from_y) * t, angle
    end
end

-- A route arrived: drive it from its first cell, sliding from where the vehicle stands to the lane.
local function take_route(vehicle)
    local cells = path.get(map_id, vehicle.request)
    release(vehicle)
    if cells == nil then
        return
    end
    vehicle.route = cells
    vehicle.index = 1
    vehicle.t = 0
    -- The way the vehicle was heading only matters when the route turns back (at a dead end).
    local entry_x, entry_y = nil, nil
    if vehicle.dir_x ~= nil and (cells[3] - cells[1]) * vehicle.dir_x + (cells[4] - cells[2]) * vehicle.dir_y < 0 then
        entry_x, entry_y = vehicle.dir_x, vehicle.dir_y
    end
    vehicle.length, vehicle.curve = cell_path(cells, 1, entry_x, entry_y)
    publish(vehicle)
    local x, y, angle = vehicle.curve(0)
    if angle ~= nil then
        vehicle.slide_x, vehicle.slide_y = vehicle.x - x, vehicle.y - y
        vehicle.slide_angle = wrap(vehicle.angle - angle)
        if math.abs(vehicle.slide_angle) > math.pi - 0.01 then
            -- Turning back: swing round through the left, not the right.
            vehicle.slide_angle = math.pi
        end
    else
        vehicle.slide_x, vehicle.slide_y, vehicle.slide_angle = 0, 0, 0
    end
end

local function poll(vehicle, dt)
    if vehicle.request ~= 0 then
        local status = path.poll(map_id, vehicle.request)
        if status == "found" then
            take_route(vehicle)
        elseif status ~= "pending" then
            -- No way there without turning back: another destination, asked for at once.
            release(vehicle)
            vehicle.dest = nil
            vehicle.retry = 0
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

-- A road of the route is gone: stop and look for another one from the cell the vehicle is in. A graph change that leaves
-- the route whole is handled at the next cell boundary (`replan`).
local function check_route(vehicle)
    local version = path.get_version(map_id)
    if vehicle.route == nil or version == vehicle.version or version == vehicle.checked then
        return
    end
    vehicle.checked = version
    local route = vehicle.route
    local last = #route // 2
    for index = vehicle.index + 1, last do
        local from_x, from_y = route[index * 2 - 3], route[index * 2 - 2]
        local to_x, to_y = route[index * 2 - 1], route[index * 2]
        if path.get_edge_cost(map_id, from_x, from_y, to_x, to_y) == nil then
            vehicle.route = nil
            publish(vehicle)
            gamestate.set("town_reroutes", gamestate.get("town_reroutes", 0) + 1)
            ask(vehicle)
            return
        end
    end
end

-- The graph changed: plan the route again from the cell the vehicle just entered, its path through that cell
-- continuing the way it came in. Without a route, the vehicle stops and asks.
local function replan(vehicle)
    local route = vehicle.route
    vehicle.version = path.get_version(map_id)
    count("town_replans")
    local entry_x, entry_y = direction(route, vehicle.index - 1)
    local cells = path.find_now(map_id, vehicle.cell_x, vehicle.cell_y, vehicle.dest[1], vehicle.dest[2],
        { no_reverse = true, heading_x = entry_x, heading_y = entry_y })
    if cells == nil or #cells < 2 then
        vehicle.route = nil
        publish(vehicle)
        ask(vehicle)
        return
    end
    if #cells < #route - (vehicle.index - 1) * 2 then
        count("town_shortcuts")
    end
    vehicle.route = cells
    vehicle.index = 1
    vehicle.t = 0
    vehicle.slide_x, vehicle.slide_y, vehicle.slide_angle = 0, 0, 0
    vehicle.length, vehicle.curve = cell_path(cells, 1, entry_x, entry_y)
    publish(vehicle)
end

-- The vehicle reached the last cell of its route.
local function arrive(vehicle)
    vehicle.route = nil
    publish(vehicle)
    gamestate.set("town_trips", gamestate.get("town_trips", 0) + 1)
    gamestate.set("town_money", gamestate.get("town_money", 0) + TRIP_PAY)
    vehicle.dest = nil
    ask(vehicle)
end

local function drive(vehicle, dt)
    local route = vehicle.route
    local last = #route // 2
    local before_x, before_y = vehicle.x, vehicle.y
    local travel = vehicle.speed * dt
    while true do
        if vehicle.length > 0 then
            vehicle.t = vehicle.t + travel / vehicle.length
        else
            vehicle.t = 1
        end
        if vehicle.t < 1 then
            break
        end
        travel = (vehicle.t - 1) * vehicle.length
        vehicle.cell_x, vehicle.cell_y = route[vehicle.index * 2 - 1], route[vehicle.index * 2]
        if vehicle.index >= last then
            local x, y, angle = vehicle.curve(1)
            if angle ~= nil then
                place(vehicle, x, y, angle)
            end
            arrive(vehicle)
            return
        end
        vehicle.index = vehicle.index + 1
        vehicle.t = 0
        vehicle.slide_x, vehicle.slide_y, vehicle.slide_angle = 0, 0, 0
        vehicle.stranded = false
        vehicle.prev_x, vehicle.prev_y = vehicle.cell_x, vehicle.cell_y
        vehicle.dir_x, vehicle.dir_y = direction(route, vehicle.index - 1)
        vehicle.cell_x, vehicle.cell_y = route[vehicle.index * 2 - 1], route[vehicle.index * 2]
        publish(vehicle)
        if path.get_version(map_id) ~= vehicle.version then
            replan(vehicle)
            route = vehicle.route
            if route == nil then
                return
            end
            last = #route // 2
        else
            vehicle.length, vehicle.curve = cell_path(route, vehicle.index)
        end
    end
    local x, y, angle = vehicle.curve(vehicle.t)
    local fade = 1 - vehicle.t
    place(vehicle, x + vehicle.slide_x * fade, y + vehicle.slide_y * fade, angle + vehicle.slide_angle * fade)
    if not vehicle.stranded and not path.is_walkable(map_id, math.floor(vehicle.x), math.floor(vehicle.y)) then
        count("town_offroad")
    end
    if isometric.get_sprite_frame(vehicle.id) ~= frame_of(vehicle, vehicle.angle) then
        count("town_frame_errors")
    end
    local moved_x, moved_y = vehicle.x - before_x, vehicle.y - before_y
    if vehicle.index > 1 and moved_x * math.cos(vehicle.angle) + moved_y * math.sin(vehicle.angle) < 0 then
        count("town_heading_errors")
    end
end

-- F3 (the `nav_debug` action) shows or hides the road graph and the route of each vehicle, which the overlay always holds.
local function update_debug()
    if input.is_action_just_pressed("nav_debug") then
        path.set_debug(not path.get_debug())
    end
    gamestate.set("town_debug", path.get_debug() and 1 or 0)
end

function on_update(dt)
    update_debug()
    for _, vehicle in ipairs(FLEET) do
        check_route(vehicle)
        poll(vehicle, dt)
        if vehicle.route ~= nil then
            vehicle.idle = 0
            drive(vehicle, dt)
        else
            vehicle.idle = (vehicle.idle or 0) + dt
            if vehicle.idle > gamestate.get("town_longest_wait", 0) then
                gamestate.set("town_longest_wait", vehicle.idle)
            end
            place(vehicle, vehicle.x, vehicle.y, vehicle.angle)
        end
    end
end
