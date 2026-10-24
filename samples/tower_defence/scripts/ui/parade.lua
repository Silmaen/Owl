-- The meadow strip under every menu of Tower defence: a wave marches along the road from the right, past the archer
-- tower, towards the keep on the left; on the victory screen fireworks burst over the keep, on the game over screen
-- the wave stands before the fallen keep, hammering at it. The sprites are the Horde sheets of the game
-- (`textures/enemies/<kind>.png`, 8 columns of headings, rows: 4 walk frames, 2 blows, 3 frames of the fall).

properties = {
    { name = "speed", type = "float", default = 2.2, min = 0 },
    { name = "span", type = "float", default = 30.0, min = 1 },
    { name = "fireworks", type = "bool", default = false },
    { name = "marching", type = "bool", default = true },
    { name = "spark_gap", type = "float", default = 0.35, min = 0.05 },
}

local DIRECTIONS = 8
local FACING_LEFT = 6       -- column of the heading toward the left of the screen
local WALK_ROWS, ATTACK_ROW = 4, 4

local walkers = {}
local sparks = {}
local clock, spark_clock = 0.0, 0.0
local seed = 7

local function roll()
    seed = (seed * 1103515245 + 12345) % 2147483648
    return (seed // 65536) % 1000 / 1000.0
end

function on_create()
    for index, id in ipairs(scene.find_entities("ParadeEnemy")) do
        local x, y, z = transform.get_position(id)
        walkers[#walkers + 1] = { id = id, x = x, y = y, z = z, phase = index * 0.9, frame = -1 }
    end
    for _, id in ipairs(scene.find_entities("Spark")) do
        sparks[#sparks + 1] = { id = id, t = 1.0, x = 0, y = 0, r = 1, g = 1, b = 1 }
    end
end

local function update_sparks(dt)
    spark_clock = spark_clock - dt
    for _, spark in ipairs(sparks) do
        if spark.t < 1.0 then
            spark.t = math.min(1.0, spark.t + dt / 0.9)
            local size = 0.6 + 2.6 * spark.t
            transform.set_position(spark.id, spark.x, spark.y + spark.t * 0.8, 0.2)
            transform.set_scale(spark.id, size, size, 1.0)
            entity.set_tint(spark.id, spark.r, spark.g, spark.b, 0.9 * (1.0 - spark.t))
        elseif spark_clock <= 0.0 then
            spark_clock = spark_gap
            spark.t = 0.0
            spark.x, spark.y = -10.5 + (roll() - 0.5) * 9.0, 0.5 + roll() * 4.5
            spark.r, spark.g, spark.b = 0.7 + roll() * 0.3, 0.4 + roll() * 0.6, 0.3 + roll() * 0.7
        end
    end
end

function on_update(dt)
    local real = time.real_delta()
    clock = clock + real
    for _, walker in ipairs(walkers) do
        if marching then
            walker.x = walker.x - speed * real
            if walker.x < -span / 2 then walker.x = walker.x + span end
        end
        local row
        if marching then
            row = math.floor(clock * 6.0 + walker.phase) % WALK_ROWS
        else
            row = ATTACK_ROW + math.floor(clock * 2.5 + walker.phase) % 2
        end
        local frame = row * DIRECTIONS + FACING_LEFT
        if frame ~= walker.frame then
            walker.frame = frame
            isometric.set_sprite_frame(walker.id, frame)
        end
        transform.set_position(walker.id, walker.x, walker.y, walker.z)
    end
    if fireworks then update_sparks(real) end
end
