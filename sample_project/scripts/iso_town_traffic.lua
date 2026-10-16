-- Isometric town traffic: vehicles driving round the ring road of scenes/iso_town.owl.
-- Demonstrates: sprites moved from a script along a path, the terrain height under them (isometric.get_height), a
--               vehicle that waits when the road ahead is bulldozed (isometric.get_cell), one sprite per axis
--               switched with ui.set_visible (the sheet holds a frame for X and one for Y), gamestate.
--
-- The ring is the rectangle of cells (6, 8) to (18, 16); a vehicle drives its middle line, clockwise on the screen, in
-- the lane on its right. Each lap pays `LAP_PAY` into `town_money` and counts in `town_laps`.

local ROAD_LAYER = 1
local LAP_PAY = 5
local LOOK_AHEAD = 0.7
local LANE = 0.2
local ROAD_X, ROAD_Y = 3, 11

-- Corners of the middle line, in cell centres, clockwise.
local CORNERS = { { 6.5, 8.5 }, { 18.5, 8.5 }, { 18.5, 16.5 }, { 6.5, 16.5 } }

-- One vehicle: its sprites along X and along Y, its speed (cells per second) and where it starts on the loop.
local FLEET = {
    { x_sprite = "CarX", y_sprite = "CarY", speed = 2.2, start = 0 },
    { x_sprite = "TruckX", y_sprite = "TruckY", speed = 1.5, start = 13 },
    { x_sprite = "CarBX", y_sprite = "CarBY", speed = 1.9, start = 27 },
}

local map_id = 0
local segments = {}
local loop_length = 0

local function build_segments()
    local from = CORNERS[1]
    local start = 0
    for index = 1, #CORNERS do
        local to = CORNERS[index % #CORNERS + 1]
        local dx, dy = to[1] - from[1], to[2] - from[2]
        local length = math.abs(dx) + math.abs(dy)
        segments[#segments + 1] = { x = from[1], y = from[2], dx = dx / length, dy = dy / length, length = length,
                                    start = start }
        start = start + length
        from = to
    end
    loop_length = start
end

-- Position and direction at a distance along the loop.
local function locate(distance)
    for _, segment in ipairs(segments) do
        if distance < segment.start + segment.length then
            local along = distance - segment.start
            return segment.x + segment.dx * along, segment.y + segment.dy * along, segment.dx, segment.dy
        end
    end
    local last = segments[#segments]
    return last.x, last.y, last.dx, last.dy
end

local function road_at(x, y)
    local _, overlay = isometric.get_cell(map_id, ROAD_LAYER, math.floor(x), math.floor(y))
    return overlay == ROAD_X or overlay == ROAD_Y
end

function on_create()
    map_id = scene.find_entity("Ground")
    build_segments()
    for _, vehicle in ipairs(FLEET) do
        vehicle.x_id = scene.find_entity(vehicle.x_sprite)
        vehicle.y_id = scene.find_entity(vehicle.y_sprite)
        vehicle.distance = vehicle.start
        vehicle.waiting = false
    end
end

local function place(vehicle)
    local x, y, dx, dy = locate(vehicle.distance)
    -- The lane on the right of the heading, then the sprite anchor: the front corner of the vehicle's cell.
    local body_x, body_y = x - dy * LANE, y + dx * LANE
    local z = isometric.get_height(map_id, body_x, body_y)
    transform.set_position(vehicle.x_id, body_x + 0.5, body_y + 0.5, z)
    transform.set_position(vehicle.y_id, body_x + 0.5, body_y + 0.5, z)
    local along_x = dx ~= 0
    ui.set_visible(vehicle.x_id, along_x)
    ui.set_visible(vehicle.y_id, not along_x)
    return x, y, dx, dy
end

function on_update(dt)
    for _, vehicle in ipairs(FLEET) do
        local x, y, dx, dy = place(vehicle)
        -- Stop in front of a missing road, go on when it is rebuilt.
        vehicle.waiting = not road_at(x + dx * LOOK_AHEAD, y + dy * LOOK_AHEAD)
        if not vehicle.waiting then
            vehicle.distance = vehicle.distance + vehicle.speed * dt
            if vehicle.distance >= loop_length then
                vehicle.distance = vehicle.distance - loop_length
                gamestate.set("town_laps", gamestate.get("town_laps", 0) + 1)
                gamestate.set("town_money", gamestate.get("town_money", 0) + LAP_PAY)
            end
        end
    end
end
