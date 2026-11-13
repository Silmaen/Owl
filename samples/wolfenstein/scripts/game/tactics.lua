-- The algorithms of the Wolfenstein combat, called from the graphs of the guards and of the arsenal: the cells of the
-- floor, the line of sight (an engine ray query), the next step of a chase over the grid and the column of a guard
-- sheet seen from the player.
--
-- A floor is the 32 x 32 grid of the hidden wall layer of the minimap (layer 0, tile 0 a wall); a door is a cell a
-- guard opens on the way (`Door`), the locked ones (`GoldDoor`, `BlueDoor`, `ElevatorDoor`) stop it.

local M = {}

local SIZE = 32
local WALL_LAYER, TILE_WALL = 0, 0
local MAX_VISITS = 700

local grid = nil   -- cell key -> true for a wall
local doors = {}   -- cell key -> {id, locked}

local function key_of(c, r)
    return r * SIZE + c
end

--- The cell of a world position: column from the west, row from the north.
---@pure
---@param x number
---@param y number
---@return integer c
---@return integer r
function M.cell_of(x, y)
    return math.floor(x + SIZE / 2), math.floor(SIZE / 2 - y)
end

--- The world position of the centre of a cell.
---@pure
---@param c integer
---@param r integer
---@return number x
---@return number y
function M.center_of(c, r)
    return c - SIZE / 2 + 0.5, SIZE / 2 - 0.5 - r
end

-- Reads the walls and the doors of the floor once per script.
local function setup()
    if grid ~= nil then
        return
    end
    grid = {}
    local map_id = scene.find_entity("Minimap")
    for r = 0, SIZE - 1 do
        for c = 0, SIZE - 1 do
            grid[key_of(c, r)] = map_id ~= 0 and isometric.get_cell(map_id, WALL_LAYER, c, r) == TILE_WALL
        end
    end
    for _, tag in ipairs({ "Door", "GoldDoor", "BlueDoor", "ElevatorDoor" }) do
        for _, id in ipairs(scene.find_entities(tag)) do
            local x, y = transform.get_position(id)
            local c, r = M.cell_of(x, y)
            doors[key_of(c, r)] = { id = id, locked = tag ~= "Door" }
        end
    end
end

local function walkable(c, r)
    if c < 0 or r < 0 or c >= SIZE or r >= SIZE or grid[key_of(c, r)] then
        return false
    end
    local door = doors[key_of(c, r)]
    return door == nil or not door.locked
end

--- The door of a cell, 0 when the cell has none.
---@param c integer
---@param r integer
---@return entity door
function M.door_at(c, r)
    setup()
    local door = doors[key_of(c, r)]
    return door and door.id or 0
end

--- The first cell of a shortest walk over the floor from one cell to another (a breadth-first search through the
--- open doors and the unlocked ones), nil when no walk joins them within the search budget.
---@param from_c integer
---@param from_r integer
---@param to_c integer
---@param to_r integer
---@return any c
---@return any r
function M.next_step(from_c, from_r, to_c, to_r)
    setup()
    if from_c == to_c and from_r == to_r then
        return to_c, to_r
    end
    local start, goal = key_of(from_c, from_r), key_of(to_c, to_r)
    local came = { [start] = start }
    local queue, head = { start }, 1
    local visits = 0
    while head <= #queue and visits < MAX_VISITS do
        local current = queue[head]
        head = head + 1
        visits = visits + 1
        if current == goal then
            local step = current
            while came[step] ~= start do
                step = came[step]
            end
            return step % SIZE, math.floor(step / SIZE)
        end
        local c, r = current % SIZE, math.floor(current / SIZE)
        for _, d in ipairs({ { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }) do
            local nc, nr = c + d[1], r + d[2]
            local next_key = key_of(nc, nr)
            if came[next_key] == nil and walkable(nc, nr) then
                came[next_key] = current
                queue[#queue + 1] = next_key
            end
        end
    end
    return nil, nil
end

--- Whether an entity sees another within a range: the first body met by a ray (`physics.raycast`) from the first
--- towards the second is the second.
---@param from_id entity
---@param to_id entity
---@param range number
---@return boolean seen
function M.can_see(from_id, to_id, range)
    local fx, fy = transform.get_position(from_id)
    local tx, ty = transform.get_position(to_id)
    local dx, dy = tx - fx, ty - fy
    local distance = math.sqrt(dx * dx + dy * dy)
    if distance > range or distance < 0.01 then
        return distance < 0.01
    end
    dx, dy = dx / distance, dy / distance
    -- the ray starts out of the body of the looker
    local hit = physics.raycast(fx + dx * 0.35, fy + dy * 0.35, dx, dy, distance)
    return hit ~= nil and hit.entity == to_id
end

--- The guard the player aims at: the nearest one in sight within `cone` radians of the view and `range` cells, 0 for
--- none, with its distance.
---@param player_id entity
---@param yaw number
---@param range number
---@param cone number
---@return entity target
---@return number distance
function M.aimed_guard(player_id, yaw, range, cone)
    local px, py = transform.get_position(player_id)
    local fx, fy = -math.sin(yaw), math.cos(yaw)
    local best, best_distance = 0, range
    for _, id in ipairs(scene.find_entities("Guard")) do
        local gx, gy = transform.get_position(id)
        local dx, dy = gx - px, gy - py
        local distance = math.sqrt(dx * dx + dy * dy)
        if distance > 0.01 and distance < best_distance then
            local cosine = (dx * fx + dy * fy) / distance
            -- a guard close by is easier to hit: the cone widens by the half width of the body
            local half = cone + math.atan(0.3 / distance)
            if cosine >= math.cos(half) and M.can_see(player_id, id, range) then
                best, best_distance = id, distance
            end
        end
    end
    return best, best_distance
end

--- The column of the 8-direction guard sheet seen from the player: 0 faces the player, 2 turns to its left, 4 shows
--- the back.
---@pure
---@param yaw number
---@param gx number
---@param gy number
---@param px number
---@param py number
---@return integer column
function M.view_column(yaw, gx, gy, px, py)
    local vx, vy = px - gx, py - gy
    local fx, fy = -math.sin(yaw), math.cos(yaw)
    local angle = math.atan(vx * fy - vy * fx, vx * fx + vy * fy)
    return math.floor((angle / (math.pi / 4)) + 0.5) % 8
end

--- The heading (raycast yaw) of a move.
---@pure
---@param dx number
---@param dy number
---@return number yaw
function M.yaw_of(dx, dy)
    return math.atan(-dx, dy)
end

return M
