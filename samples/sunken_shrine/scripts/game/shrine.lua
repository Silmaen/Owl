-- The few things of the Sunken Shrine a graph would hide, for `room.owlvs` and `player.owlvs`: the kind of an entity
-- read from its name, the facing of the hero as one of four directions and its picture, the stone block pushed one
-- cell, the room bounds a creature stays in, the push, the blink and the camera shake of a hit on the hero.

local M = {}

--- The kind of an entity: its name without the number that tells the copies apart (`Brazier3` is a `Brazier`).
---@pure
---@param name string
---@return string kind
function M.kind(name)
    return (string.gsub(name or "", "%d+$", ""))
end

local USABLE = { OldMan = true, Chest = true, SwordStand = true, Brazier = true, Block = true }

--- The name of the first thing among some entities the hero can use (the old man, the chest, the sword, a brazier,
--- the block); `none` when there is none.
---@pure
---@param ids table
---@return string name
function M.usable(ids)
    for _, id in ipairs(ids or {}) do
        local name = entity.get_name(id)
        if USABLE[M.kind(name)] then return name end
    end
    return "none"
end

local CREATURES = { Slime = true, Bat = true, Spider = true, Rat = true, Guardian = true }

--- The kind of the first creature among some entities, `none` when there is none.
---@pure
---@param ids table
---@return string kind
function M.creature(ids)
    for _, id in ipairs(ids or {}) do
        local kind = M.kind(entity.get_name(id))
        if CREATURES[kind] then return kind end
    end
    return "none"
end

--- The number of an entity at the end of its name (`Brazier3` is 3), 0 without one.
---@pure
---@param name string
---@return integer number
function M.number(name)
    return tonumber(string.match(name or "", "(%d+)$")) or 0
end

--- Distance between two entities; far when one of them is missing.
---@pure
---@param a entity
---@param b entity
---@return number distance
function M.distance(a, b)
    if a == 0 or b == 0 or a == nil or b == nil then return 1000 end
    local ax, ay = transform.get_position(a)
    local bx, by = transform.get_position(b)
    return math.sqrt((ax - bx) ^ 2 + (ay - by) ^ 2)
end

--- How many of the braziers 1 to `count` burn (the `shrine_lit_<n>` game state keys).
---@pure
---@param count integer
---@return integer lit
function M.lit(count)
    local lit = 0
    for index = 1, count do
        if gamestate.get("shrine_lit_" .. index, false) then lit = lit + 1 end
    end
    return lit
end

--- One of the four directions closest to a direction, as a unit vector (down when it is zero).
---@pure
---@param direction vector2
---@return vector2 axis
function M.axis(direction)
    local x, y = direction.x or 0, direction.y or 0
    if math.abs(x) < 1e-3 and math.abs(y) < 1e-3 then return { x = 0, y = -1 } end
    if math.abs(x) > math.abs(y) then return { x = x > 0 and 1 or -1, y = 0 } end
    return { x = 0, y = y > 0 and 1 or -1 }
end

--- Push the block one cell along a direction when the cell is free (no wall, creature or thing in it).
---@param block entity
---@param direction vector2
---@return boolean moved
function M.push(block, direction)
    local step = M.axis(direction)
    local x, y = transform.get_position(block)
    -- The cells of a room are centred on whole numbers.
    local tx, ty = math.floor(x + step.x + 0.5), math.floor(y + step.y + 0.5)
    for _, id in ipairs(physics.overlap_box(tx, ty, 0.4, 0.4, 0)) do
        if id ~= block then return false end
    end
    physics.set_transform(block, tx, ty, 0)
    transform.set_position(block, tx, ty, 0.05)
    return true
end

--- Whether two entities stand on the same cell.
---@pure
---@param a entity
---@param b entity
---@return boolean same
function M.same_cell(a, b)
    local ax, ay = transform.get_position(a)
    local bx, by = transform.get_position(b)
    return math.abs(ax - bx) < 0.3 and math.abs(ay - by) < 0.3
end

--- Keep an entity inside the walls of the room: creatures fly over the floor, not through the walls.
---@param id entity
---@param half_width number
---@param half_height number
function M.keep_inside(id, half_width, half_height)
    local x, y, z = transform.get_position(id)
    local cx = math.max(-half_width, math.min(half_width, x))
    local cy = math.max(-half_height, math.min(half_height, y))
    if cx ~= x or cy ~= y then
        physics.set_transform(id, cx, cy, 0)
        transform.set_position(id, cx, cy, z)
    end
end

--- The frame of the hero sheet (`textures/fx/hero.png`) for the way it faces: 0 down, 1 up, 2 left, 3 right.
---@pure
---@param face vector2
---@return integer frame
function M.facing_frame(face)
    local fx, fy = face.x or 0, face.y or -1
    if math.abs(fx) > math.abs(fy) then return fx < 0 and 2 or 3 end
    return fy > 0 and 1 or 0
end

-- The seconds of grace left after a hit above which the hero still shows the red of the hit (the grace is 1.5 s, 2 s
-- with gentle creatures); below, it blinks until the grace is over.
local HURT_FLASH = 1.2

--- How the hero looks from the seconds left of its grace after a hit: deep red at first, then blinking (its alpha
--- dropping ten times a second) while no creature can hurt it.
---@pure
---@param left number
---@return number shade
---@return number alpha
function M.hurt_look(left)
    if left == nil or left <= 0 then return 1, 1 end
    if left > HURT_FLASH then return 0.25, 1 end
    return 1, (math.floor(left * 10) % 2 == 0) and 0.2 or 1
end

-- The half extent of the floor of a room the hero may be pushed within (the creatures keep to the same).
local FLOOR_HALF_WIDTH, FLOOR_HALF_HEIGHT = 6.2, 3.2

-- A push along one axis cut so that, applied for some seconds, it ends within the floor (never out by a door).
local function within(position, push, time, half)
    local reach = position + push * time
    if reach > half then return math.max(0, half - position) / time end
    if reach < -half then return math.min(0, -half - position) / time end
    return push
end

--- The push a creature gives the hero it touches, as a velocity held for `time` seconds: `strength` long, away from
--- the first creature among some entities (down when none is found), cut to stay on the floor of the room.
---@pure
---@param hero entity
---@param ids table
---@param strength number
---@param time number
---@return vector2 push
function M.knock(hero, ids, strength, time)
    local hx, hy = transform.get_position(hero)
    hx, hy = hx or 0, hy or 0
    local px, py = 0, -strength
    for _, id in ipairs(ids or {}) do
        if CREATURES[M.kind(entity.get_name(id))] then
            local cx, cy = transform.get_position(id)
            local dx, dy = hx - (cx or 0), hy - (cy or 0)
            local n = math.sqrt(dx * dx + dy * dy)
            if n > 1e-3 then px, py = dx / n * strength, dy / n * strength end
            break
        end
    end
    time = math.max(time or 0.15, 1e-3)
    return { x = within(hx, px, time, FLOOR_HALF_WIDTH), y = within(hy, py, time, FLOOR_HALF_HEIGHT) }
end

-- The camera shake: its strength (0 to 1, fading), the clock of its wobble, where the camera stood before it and
-- where the shake last put it (a camera found elsewhere is the one of a new room).
local trauma, wobble, home, placed = 0.0, 0.0, nil, nil

--- Shake the room camera by an amount (a hit on the hero is 0.5), none with the `reduced_motion` setting.
---@param amount number
function M.jolt(amount)
    gamestate.set("shrine_jolts", gamestate.get("shrine_jolts", 0) + 1)
    if settings.get("reduced_motion", false) == true then return end
    trauma = math.min(1, trauma + amount)
end

--- Move the room camera by the shake left this frame, and back where it stood once the shake is over.
---@param camera entity
---@param dt number
function M.settle(camera, dt)
    if camera == nil or camera == 0 then return end
    local x, y, z = transform.get_position(camera)
    if home ~= nil and (placed == nil or math.abs(x - placed[1]) > 1e-3 or math.abs(y - placed[2]) > 1e-3) then
        home = nil
    end
    if trauma <= 0 then
        if home ~= nil then transform.set_position(camera, home[1], home[2], home[3]) end
        home, placed = nil, nil
        return
    end
    if home == nil then home = { x, y, z } end
    trauma = math.max(0, trauma - dt * 2.0)
    wobble = wobble + dt
    local k = trauma * trauma * 0.3
    placed = { home[1] + k * math.sin(wobble * 53), home[2] + k * math.sin(wobble * 61 + 1.7) }
    transform.set_position(camera, placed[1], placed[2], home[3])
end

--- Pose the stroke around the hero and show the picture of the way it faces. While a stroke lasts (`left` of its
--- `length` seconds), the blade sweeps an arc of 150 degrees across the facing direction and the white arc fades in
--- front of the hero.
---@param hero entity
---@param slash entity
---@param blade entity
---@param face vector2
---@param reach number
---@param left number
---@param length number
---@return boolean striking
function M.pose(hero, slash, blade, face, reach, left, length)
    local x, y = transform.get_position(hero)
    local fx, fy = face.x or 0, face.y or -1
    local aim = math.atan(fy, fx)
    local striking = left > 0
    local frame = M.facing_frame(face)
    isometric.set_sprite_frame(hero, frame)
    gamestate.set("shrine_hero_frame", frame)
    gamestate.set("shrine_striking", striking)
    ui.set_visible(slash, striking)
    ui.set_visible(blade, striking)
    if striking then
        local done = 1 - math.min(1, left / length)
        local sweep = aim + math.rad(75) - done * math.rad(150)
        -- The sword of the sheet points up: its blade is turned onto the radius of the arc.
        transform.set_position(blade, x + math.cos(sweep) * reach * 0.75, y + math.sin(sweep) * reach * 0.75, 0.25)
        transform.set_rotation(blade, 0, 0, sweep - math.pi / 2)
        transform.set_position(slash, x + fx * reach, y + fy * reach, 0.2)
        transform.set_rotation(slash, 0, 0, aim)
        entity.set_tint(slash, 1, 1, 1, 1 - done * 0.8)
    end
    return striking
end

return M
