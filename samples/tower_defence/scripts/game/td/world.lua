-- The pads, the roads (waypoints) and the banners of the level.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data
local World = td.world

-- The pads, numbered row by row from the north-west (the numbers of `start_towers` and of the scenarios).
function World.find_pads()
    for _, id in ipairs(scene.find_entities("Pad")) do
        local x, y, z = transform.get_position(id)
        local level = math.max(0, math.floor((y - C.GRASS_TOP) / height_step + 0.5))
        local pad = { id = id, x = x, y = y, z = z, level = level, tower = nil }
        S.pads[#S.pads + 1] = pad
        S.pad_by_id[id] = pad
    end
    table.sort(S.pads, function(a, b)
        if math.abs(a.z - b.z) > 0.01 then return a.z < b.z end
        return a.x < b.x
    end)
    for index, pad in ipairs(S.pads) do pad.index = index end
end

-- The roads: `Waypoint NN` (road A), then `Waypoint B NN` (road B, a level with two gates), in the order of their
-- numbers, with the distance walked at each point.
function World.read_road(prefix)
    local points = {}
    for number = 1, 99 do
        local id = scene.find_entity(string.format("Waypoint %s%02d", prefix, number))
        if id ~= 0 then
            local x, y, z = transform.get_position(id)
            points[#points + 1] = { x = x, y = y, z = z, at = 0.0 }
        end
    end
    if #points < 2 then return nil end
    local road = { points = points, total = 0.0 }
    for i = 2, #points do
        local a, b = points[i - 1], points[i]
        local dx, dy, dz = b.x - a.x, b.y - a.y, b.z - a.z
        road.total = road.total + math.sqrt(dx * dx + dy * dy + dz * dz)
        b.at = road.total
    end
    return road
end

function World.find_route()
    S.routes[1] = World.read_road("")
    S.routes[2] = World.read_road("B ")
    S.route = S.routes[1]
    if S.route == nil then
        log.warn("Tower defence: the level needs at least two Waypoint NN entities.")
        return
    end
    gamestate.set("tower_route", #S.route.points)
    gamestate.set("tower_roads", S.routes[2] ~= nil and 2 or 1)
end

-- The point of a road (1 or 2) at a distance from its gate and the direction of the road there (on the ground plane).
function World.along(distance, road)
    local points = (S.routes[road or 1] or S.route).points
    local index = 1
    while index < #points - 1 and points[index + 1].at < distance do index = index + 1 end
    local a, b = points[index], points[index + 1]
    local length = b.at - a.at
    local t = length > 0 and math.max(0.0, math.min(1.0, (distance - a.at) / length)) or 0.0
    local dx, dz = b.x - a.x, b.z - a.z
    local flat = math.sqrt(dx * dx + dz * dz)
    if flat > 0 then dx, dz = dx / flat, dz / flat end
    return a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, dx, dz
end

-- The banners of the keep and of the gate wave in the wind.
function World.wave_banners()
    for index, banner in ipairs(S.banner_ids) do
        local sway = math.sin(S.clock * 2.3 + index * 1.7)
        transform.set_rotation(banner.id, 0.04 * sway, banner.yaw + 0.22 * sway, 0.03 * math.sin(S.clock * 3.1 + index))
    end
end

return World
