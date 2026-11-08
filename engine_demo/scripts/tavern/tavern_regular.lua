-- A regular of the tavern strolling from spot to spot of the room on its navigation mesh: round the tables, the server
-- on her round, the other regular and the player; a spot on top of the second table is reached by the TableHop link.
-- Demonstrates: a NavAgent on the baked mesh of a NavMeshVolume (nav.go_to, nav.is_moving), the avoidance of the
-- other agents and of the NavObstacles, a NavLink between two floors, the speed driving the Animator of the child mesh.

properties = {
    { name = "spots", type = "string", default = "-2.4 0 -1.8; 0.4 0 1.6; 2.1 0 2.9" },
    { name = "pause", type = "float", default = 2.5 },
}

local points = {}
local resting = 0
local current = 0

local function read_spots(text)
    local list = {}
    for item in string.gmatch(text, "[^;]+") do
        local x, y, z = string.match(item, "(%-?[%d%.]+)%s+(%-?[%d%.]+)%s+(%-?[%d%.]+)")
        if x then
            list[#list + 1] = { tonumber(x), tonumber(y), tonumber(z) }
        end
    end
    return list
end

local function walk_to_another()
    if #points < 2 then
        return
    end
    local next_spot = current
    while next_spot == current do
        next_spot = math.random(#points)
    end
    current = next_spot
    local spot = points[current]
    nav.go_to(entity_id, spot[1], spot[2], spot[3])
end

function on_create()
    points = read_spots(spots)
    resting = pause
end

function on_update(dt)
    if nav.is_moving(entity_id) then
        return
    end
    resting = resting - dt
    if resting <= 0 then
        resting = pause
        if current ~= 0 then
            gamestate.set("tavern_strolls", gamestate.get("tavern_strolls", 0) + 1)
        end
        walk_to_another()
    end
end
