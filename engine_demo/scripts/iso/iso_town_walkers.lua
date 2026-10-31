-- Pedestrians of the isometric town plaza: each one strolls to a random point of the paved plaza, waits there a moment,
-- then sets off again, around the buildings and the lamp that stand on it.
-- Demonstrates: a `WalkArea` (the plaza, its floor following the buildings, a hole round the lamp) on the plane of the
--               isometric map, `WalkAgent`s sent with `walk.go_to`, polled with `walk.is_moving`, the points picked
--               with `walk.is_inside`, and the steps of an `AnimatedSpriteRenderer` sheet picked with
--               `isometric.set_sprite_frame` (four steps, a row heading right and a row heading left). F3 draws the
--               floor and the path of every walker.
--
-- Entities by tag: `Plaza` (the area) and `Walker_<n>`. The gamestate key `town_walks` counts the walks set off, for
-- the scenario test.

properties = {
    { name = "walkers", type = "int", default = 5, min = 1 },        -- walkers named `Walker_1` to `Walker_<n>`
    { name = "pause_min", type = "float", default = 1.0, min = 0 },  -- seconds a walker waits at a point, at least
    { name = "pause_max", type = "float", default = 4.0, min = 0 },
    { name = "step_rate", type = "float", default = 7.0, min = 0 },  -- steps of the sheet per second while walking
    { name = "stroll", type = "float", default = 5.0, min = 0.5 },   -- farthest a walk goes, in cells along x and y
}

local plaza = 0
local list = {}
local seed = 12345
local walks = 0
local clock = 0

local function random()
    seed = (seed * 1103515245 + 12345) % 2147483648
    return seed / 2147483648
end

-- A point of the plaza at most `stroll` cells away along each axis.
local function pick_point(from_x, from_y)
    for _ = 1, 30 do
        local x = from_x + (random() * 2 - 1) * stroll
        local y = from_y + (random() * 2 - 1) * stroll
        if walk.is_inside(plaza, x, y) then
            return x, y
        end
    end
    return nil
end

function on_create()
    plaza = scene.find_entity("Plaza")
    for index = 1, walkers do
        local id = scene.find_entity("Walker_" .. index)
        if id ~= 0 then
            local x, y = transform.get_position(id)
            list[#list + 1] = { id = id, wait = random() * pause_max, last_screen = x - y, row = 0 }
        end
    end
    gamestate.set("town_walks", 0)
end

function on_update(dt)
    clock = clock + dt
    for _, walker in ipairs(list) do
        local x, y = transform.get_position(walker.id)
        -- On the screen of the isometric view, x - y grows to the right.
        local screen = x - y
        if walk.is_moving(walker.id) then
            if screen < walker.last_screen - 1e-4 then
                walker.row = 1
            elseif screen > walker.last_screen + 1e-4 then
                walker.row = 0
            end
            isometric.set_sprite_frame(walker.id, walker.row * 4 + math.floor(clock * step_rate) % 4)
        else
            isometric.set_sprite_frame(walker.id, walker.row * 4)
            walker.wait = walker.wait - dt
            if walker.wait <= 0 then
                local tx, ty = pick_point(x, y)
                if tx ~= nil and walk.go_to(walker.id, tx, ty) then
                    walks = walks + 1
                    gamestate.set("town_walks", walks)
                end
                walker.wait = pause_min + random() * (pause_max - pause_min)
            end
        end
        walker.last_screen = screen
    end
end
