-- The pads, the roads (a spline each, its lanes side by side) and the banners of the level.

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

-- A road: the entity of its `Spline` (`Road`, or `Road B` for the second gate of a level), its length and its
-- number of points.
function World.read_road(tag)
    local id = scene.find_entity(tag)
    local total = id ~= 0 and spline.get_length(id) or nil
    if total == nil or total <= 0 then return nil end
    return { id = id, total = total, points = spline.get_point_count(id) or 0 }
end

function World.find_route()
    S.routes[1] = World.read_road("Road")
    S.routes[2] = World.read_road("Road B")
    S.route = S.routes[1]
    if S.route == nil then
        log.warn("Tower defence: the level needs a Road entity with a Spline of two points or more.")
        return
    end
    gamestate.set("tower_route", S.route.points)
    gamestate.set("tower_roads", S.routes[2] ~= nil and 2 or 1)
end

-- The point of a road (1 or 2) at a distance from its gate, in a lane when named, and the direction of the road there
-- (on the ground plane).
function World.along(distance, road, lane)
    local id = (S.routes[road or 1] or S.route).id
    local x, y, z = spline.sample(id, distance, lane)
    local dx, _, dz = spline.get_tangent(id, distance)
    local flat = math.sqrt(dx * dx + dz * dz)
    if flat > 0 then dx, dz = dx / flat, dz / flat end
    return x, y, z, dx, dz
end

-- The banners of the keep and of the gate wave in the wind.
function World.wave_banners()
    if S.still then return end
    for index, banner in ipairs(S.banner_ids) do
        local sway = math.sin(S.clock * 2.3 + index * 1.7)
        transform.set_rotation(banner.id, 0.04 * sway, banner.yaw + 0.22 * sway, 0.03 * math.sin(S.clock * 3.1 + index))
    end
end

return World
