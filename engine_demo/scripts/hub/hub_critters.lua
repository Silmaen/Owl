-- Small life of the hub: butterflies fluttering over the gardens and sparrows crossing the sky now and then.
-- Demonstrates: moving many entities from one script (`scene.find_entity` once, `transform.set_*` every frame), sprites whose
--               two wing frames loop on their own (`AnimatedSpriteRenderer` with `loop: true`), sine paths, a sprite mirrored
--               toward its flight.
--
-- Entities named `Butterfly_<n>` stay near the position they have in the scene; `Sparrow_<n>` wait off the map, then fly
-- over it in a straight line, one after the other.

properties = {
    { name = "sparrow_speed", type = "float", default = 5.5 },
}

local BUTTERFLIES = 8
local SPARROWS = 3
local MAP_W = 64
local SPARROW_SPEED    -- set from the properties in on_create

local flies = {}
local birds = {}
local clock = 0

function on_create()
    SPARROW_SPEED = sparrow_speed
    for n = 1, BUTTERFLIES do
        local id = scene.find_entity("Butterfly_" .. n)
        if id ~= 0 then
            local x, y, z = transform.get_position(id)
            flies[#flies + 1] = { id = id, x = x, y = y, z = z, phase = n * 1.7, size = 0.8 }
        end
    end
    for n = 1, SPARROWS do
        local id = scene.find_entity("Sparrow_" .. n)
        if id ~= 0 then
            birds[#birds + 1] = { id = id, wait = 4 + n * 5.5, x = -5, y = -10, dx = 1, dy = 0 }
        end
    end
end

local function launch(bird)
    -- From a side of the map to the opposite one, at a height of the sky that changes every time.
    local from_left = math.random() < 0.5
    bird.x = from_left and -4 or MAP_W + 4
    bird.dx = from_left and 1 or -1
    bird.y = -(6 + math.random() * 28)
    bird.dy = (math.random() - 0.5) * 0.3
    bird.wait = 0
end

function on_update(dt)
    clock = clock + dt
    for _, fly in ipairs(flies) do
        local t = clock * 0.8 + fly.phase
        local x = fly.x + math.sin(t) * 1.8 + math.sin(t * 2.3) * 0.5
        local y = fly.y + math.cos(t * 0.9) * 1.2 + math.sin(t * 3.1) * 0.25
        transform.set_position(fly.id, x, y, fly.z)
        local heading = math.cos(t) >= 0 and fly.size or -fly.size
        transform.set_scale(fly.id, heading, fly.size, 1)
    end
    for _, bird in ipairs(birds) do
        if bird.wait > 0 then
            bird.wait = bird.wait - dt
            if bird.wait <= 0 then
                launch(bird)
            end
        else
            bird.x = bird.x + bird.dx * SPARROW_SPEED * dt
            bird.y = bird.y + bird.dy * SPARROW_SPEED * dt + math.sin(clock * 6 + bird.dx) * 0.01
            if bird.x < -6 or bird.x > MAP_W + 6 then
                bird.wait = 18 + math.random() * 20
                bird.x, bird.y = -10, -10
            end
        end
        transform.set_position(bird.id, bird.x, bird.y, 0.85)
        transform.set_scale(bird.id, bird.dx, 1, 1)
    end
end
