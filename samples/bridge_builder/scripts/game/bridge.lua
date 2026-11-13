-- The bridge of a Bridge Builder level, for the graph `level.owlvs`: the nodes and the parts the player lays on the
-- build grid, the undo stack, the cost, the saved design, and the bodies and joints of a test run. A graph would hide
-- these lists and loops.
--
-- The bridge lives in the plane z = 0, seen from the side: x to the right, y up, one grid cell per metre. A node is a
-- point of the grid; the `Anchor` entities of the scene are the fixed nodes on the cliffs. A part joins two nodes:
-- `road` (the deck the vehicles drive on), `wood` and `steel` (the supports, that no vehicle touches).

local M = {}

-- Kinds of parts: cost per metre, longest part, break force of its joints, prefabs of the plan and of the run.
M.KINDS = {
    road = { cost = 100, max = 3, strength = 11000, label = "ROAD" },
    wood = { cost = 60, max = 4, strength = 8500, label = "WOOD" },
    steel = { cost = 160, max = 5, strength = 21000, label = "STEEL" },
}
M.ORDER = { "road", "wood", "steel" }

-- Every prefab the build spawns, one path a line, so that the export finds them all.
local PREFABS = {
    node = "prefabs/node.owlprefab",
    anchor = "prefabs/anchor_node.owlprefab",
    plan_road = "prefabs/plan_road.owlprefab",
    plan_wood = "prefabs/plan_wood.owlprefab",
    plan_steel = "prefabs/plan_steel.owlprefab",
    road = "prefabs/road.owlprefab",
    wood = "prefabs/wood.owlprefab",
    steel = "prefabs/steel.owlprefab",
    grid = "prefabs/grid_line.owlprefab",
}

-- How far the camera moves from where the level puts it: zoom between these fractions of its distance, pan up to the
-- edge of the first view plus a margin (metres, across and up).
local ZOOM_MIN, ZOOM_MAX = 0.55, 1.15
local PAN_MARGIN_X, PAN_MARGIN_Y = 3, 2

local stick_clock = 0
local last_mouse = { -1, -1 }
local grid_lines = {}

local S = {
    nodes = {}, -- { x, y, anchor (entity or 0), visual (entity or 0) }
    parts = {}, -- { a, b, kind, visual, body, tail }
    undo = {},
    kind = "road",
    cx = 0, cy = 0, -- the cursor, in grid cells
    drag = nil, -- the node a part is started from
    bounds = { -3, -9, 23, 7 },
    running = false,
    broken = 0,
    home = nil, -- where the level puts the camera: { x, y, z }
    follow = false, -- the cursor moved by keys or stick: keep it in view
    pan_from = nil, -- the mouse at the previous frame of a drag of the view
}

local function node_at(x, y)
    for index, node in ipairs(S.nodes) do
        if node.x == x and node.y == y then return index end
    end
    return nil
end

local function length(a, b)
    local na, nb = S.nodes[a], S.nodes[b]
    return math.sqrt((na.x - nb.x) ^ 2 + (na.y - nb.y) ^ 2)
end

-- Place an entity of unit length along X between two points of the plane, its +Y up whichever end comes first (the
-- asphalt of a road faces the sky however the part is drawn), or from the first point to the second when `as_given`.
local function stretch(id, ax, ay, bx, by, thickness, as_given)
    if not as_given and (bx < ax or (bx == ax and by < ay)) then
        ax, ay, bx, by = bx, by, ax, ay
    end
    local dx, dy = bx - ax, by - ay
    transform.set_position(id, (ax + bx) / 2, (ay + by) / 2, 0)
    transform.set_rotation(id, 0, 0, math.atan(dy, dx))
    transform.set_scale(id, math.sqrt(dx * dx + dy * dy), thickness or 1, 1)
end

local function show_node(index)
    local node = S.nodes[index]
    if node.visual == 0 then
        node.visual = scene.spawn_prefab(node.anchor ~= 0 and PREFABS.anchor or PREFABS.node, node.x, node.y, 0)
    end
end

local function add_node(x, y)
    S.nodes[#S.nodes + 1] = { x = x, y = y, anchor = 0, visual = 0 }
    show_node(#S.nodes)
    return #S.nodes
end

local function show_part(part)
    if part.visual == 0 then part.visual = scene.spawn_prefab(PREFABS["plan_" .. part.kind], 0, 0, 0) end
    local a, b = S.nodes[part.a], S.nodes[part.b]
    stretch(part.visual, a.x, a.y, b.x, b.y)
end

local function used(index)
    for _, part in ipairs(S.parts) do
        if part.a == index or part.b == index then return true end
    end
    return S.nodes[index].anchor ~= 0
end

-- Remove the nodes no part holds any more, with their bolts, wherever they are in the list; the parts keep pointing
-- at their nodes.
local function drop_loose_nodes()
    for index = #S.nodes, 1, -1 do
        if not used(index) then
            if S.nodes[index].visual ~= 0 then scene.destroy_entity(S.nodes[index].visual) end
            table.remove(S.nodes, index)
            for _, part in ipairs(S.parts) do
                if part.a > index then part.a = part.a - 1 end
                if part.b > index then part.b = part.b - 1 end
            end
        end
    end
end

-- A part runs from its left node to its right one (from the lower one when upright), whichever way it was drawn;
-- `back` tells it was drawn from right to left.
local function add_part(a, b, kind)
    local na, nb = S.nodes[a], S.nodes[b]
    local back = nb.x < na.x or (nb.x == na.x and nb.y < na.y)
    if back then a, b = b, a end
    local part = { a = a, b = b, kind = kind, visual = 0, body = 0, tail = 0, look = 0, back = back }
    S.parts[#S.parts + 1] = part
    show_part(part)
    return part
end

--- Read the anchors of the level (the `Anchor` entities, their x and y rounded to the grid) and the bounds of the
--- build grid, then lay the saved design down when there is one.
---@param min_x integer
---@param min_y integer
---@param max_x integer
---@param max_y integer
---@param design string
---@return integer anchors
function M.setup(min_x, min_y, max_x, max_y, design)
    S.bounds = { min_x, min_y, max_x, max_y }
    -- The mouse moves the cursor only once it moves.
    last_mouse[1], last_mouse[2] = scene.get_viewport_mouse()
    S.nodes, S.parts, S.undo, S.drag, S.running, S.broken = {}, {}, {}, nil, false, 0
    S.home, S.follow, S.pan_from, grid_lines = nil, false, nil, {}
    for _, id in ipairs(scene.find_entities("Anchor")) do
        local x, y = transform.get_world_position(id)
        S.nodes[#S.nodes + 1] = { x = math.floor(x + 0.5), y = math.floor(y + 0.5), anchor = id, visual = 0 }
        show_node(#S.nodes)
    end
    -- The cursor starts on the anchor at the top of the left cliff, the origin of the level.
    S.cx, S.cy = 0, 0
    M.load(design)
    return #S.nodes
end

-- The nodes of a part in the order it was drawn.
local function drawn(part)
    if part.back then return S.nodes[part.b], S.nodes[part.a] end
    return S.nodes[part.a], S.nodes[part.b]
end

--- The design as text, to keep in the settings: `kind ax ay bx by;` per part, from the node it was drawn from.
---@pure
---@return string design
function M.save()
    local out = {}
    for _, part in ipairs(S.parts) do
        local a, b = drawn(part)
        out[#out + 1] = string.format("%s %d %d %d %d", part.kind, a.x, a.y, b.x, b.y)
    end
    return table.concat(out, ";")
end

--- Lay down a design written by `save`; parts that no longer fit are left out.
---@param design string
function M.load(design)
    if design == nil or design == "" or design == "none" then return end
    for item in string.gmatch(design, "[^;]+") do
        local kind, ax, ay, bx, by = string.match(item, "(%a+) (%-?%d+) (%-?%d+) (%-?%d+) (%-?%d+)")
        if kind and M.KINDS[kind] then
            ax, ay, bx, by = tonumber(ax), tonumber(ay), tonumber(bx), tonumber(by)
            local a = node_at(ax, ay) or add_node(ax, ay)
            local b = node_at(bx, by) or add_node(bx, by)
            add_part(a, b, kind)
        end
    end
end

--- Move the cursor by whole cells, kept on the grid.
---@param dx integer
---@param dy integer
function M.move_cursor(dx, dy)
    S.cx = math.max(S.bounds[1], math.min(S.bounds[3], S.cx + dx))
    S.cy = math.max(S.bounds[2], math.min(S.bounds[4], S.cy + dy))
    S.follow = true
end

--- Move the cursor with a stick: one cell at once, then one every `repeat_time` seconds while it is held.
---@param stick vector2
---@param repeat_time number
---@param dt number
function M.stick(stick, repeat_time, dt)
    local dx = stick.x > 0.5 and 1 or (stick.x < -0.5 and -1 or 0)
    local dy = stick.y > 0.5 and -1 or (stick.y < -0.5 and 1 or 0)
    if dx == 0 and dy == 0 then
        stick_clock = 0
        return
    end
    stick_clock = stick_clock - dt
    if stick_clock <= 0 then
        M.move_cursor(dx, dy)
        stick_clock = repeat_time
    end
end

--- Put the cursor on the grid point under the mouse when the mouse moved: the ray of the camera through the mouse
--- meets the plane z = 0.
---@param camera entity
---@param fov number
---@return boolean moved
function M.mouse_cursor(camera, fov)
    local mx, my = scene.get_viewport_mouse()
    if mx == last_mouse[1] and my == last_mouse[2] then return false end
    last_mouse[1], last_mouse[2] = mx, my
    local width, height = scene.get_viewport_size()
    if width <= 0 or height <= 0 or mx < 0 or my < 0 or mx > width or my > height then return false end
    local px, py, pz = transform.get_world_position(camera)
    local t = math.tan(fov / 2)
    local nx = (mx / width * 2 - 1) * t * width / height
    local ny = (1 - my / height * 2) * t
    -- The camera of a level looks straight along -Z.
    local x, y = px + nx * pz, py + ny * pz
    local gx = math.max(S.bounds[1], math.min(S.bounds[3], math.floor(x + 0.5)))
    local gy = math.max(S.bounds[2], math.min(S.bounds[4], math.floor(y + 0.5)))
    local moved = gx ~= S.cx or gy ~= S.cy
    S.cx, S.cy = gx, gy
    return moved
end

--- Move the camera a little: zoom (`zoom` held, the wheel), pan (`pan`, a drag of the view with the mouse), the
--- cursor moved by keys or stick kept in view; within `ZOOM_MIN` to `ZOOM_MAX` of the distance the level sets and a
--- few metres past its first view. The camera keeps looking straight along -Z.
---@param camera entity
---@param fov number
---@param pan vector2
---@param zoom number
---@param drag boolean
---@param wheel vector2
---@param dt number
---@return number distance
function M.view(camera, fov, pan, zoom, drag, wheel, dt)
    local x, y, z = transform.get_world_position(camera)
    if S.home == nil then S.home = { x = x, y = y, z = z } end
    local home = S.home
    local width, height = scene.get_viewport_size()
    local aspect = (width > 0 and height > 0) and width / height or 16 / 9
    local t = math.tan(fov / 2)
    local by_mouse = wheel.y ~= 0
    z = math.max(home.z * ZOOM_MIN, math.min(home.z * ZOOM_MAX, z * math.exp(-(zoom * 1.2 * dt + wheel.y * 0.12))))
    x = x + pan.x * z * t * dt
    y = y - pan.y * z * t * dt
    local mx, my = scene.get_viewport_mouse()
    if drag and height > 0 then
        if S.pan_from ~= nil then
            local metres = 2 * z * t / height
            x = x - (mx - S.pan_from[1]) * metres
            y = y + (my - S.pan_from[2]) * metres
            by_mouse = true
        end
        S.pan_from = { mx, my }
    else
        S.pan_from = nil
    end
    local half_w, half_h = z * t * aspect, z * t
    if S.follow then
        local margin = 1.5
        x = math.max(S.cx - half_w + margin, math.min(S.cx + half_w - margin, x))
        y = math.max(S.cy - half_h + margin, math.min(S.cy + half_h - margin, y))
        S.follow = false
    end
    local reach_x = math.max(0, home.z * t * aspect - half_w) + PAN_MARGIN_X
    local reach_y = math.max(0, home.z * t - half_h) + PAN_MARGIN_Y
    x = math.max(home.x - reach_x, math.min(home.x + reach_x, x))
    y = math.max(home.y - reach_y, math.min(home.y + reach_y, y))
    transform.set_position(camera, x, y, z)
    -- The grid point under a still mouse changes when the mouse zooms or drags the view.
    if by_mouse then last_mouse[1], last_mouse[2] = -1, -1 end
    return z / home.z
end

--- Show or hide the build grid: a faint line along every metre of the bounds, in front of the parts.
---@param visible boolean
---@return boolean visible
function M.show_grid(visible)
    if visible and #grid_lines == 0 then
        local min_x, min_y, max_x, max_y = S.bounds[1], S.bounds[2], S.bounds[3], S.bounds[4]
        for gx = min_x, max_x do
            local id = scene.spawn_prefab(PREFABS.grid, 0, 0, 0)
            stretch(id, gx, min_y, gx, max_y, 1)
            grid_lines[#grid_lines + 1] = id
        end
        for gy = min_y, max_y do
            local id = scene.spawn_prefab(PREFABS.grid, 0, 0, 0)
            stretch(id, min_x, gy, max_x, gy, 1)
            grid_lines[#grid_lines + 1] = id
        end
    elseif not visible then
        for _, id in ipairs(grid_lines) do scene.destroy_entity(id) end
        grid_lines = {}
    end
    return visible
end

--- Number of the nodes laid by the player (the anchors left out).
---@pure
---@return integer nodes
function M.node_count()
    local count = 0
    for _, node in ipairs(S.nodes) do
        if node.anchor == 0 then count = count + 1 end
    end
    return count
end

--- The cursor, in grid cells.
---@pure
---@return vector2 cell
function M.cursor()
    return { x = S.cx, y = S.cy }
end

--- Choose the kind of the next parts.
---@param kind string
---@return string label
function M.set_kind(kind)
    if M.KINDS[kind] then S.kind = kind end
    return M.KINDS[S.kind].label
end

--- The next kind of part, in the order road, wood, steel.
---@return string label
function M.next_kind()
    for index, name in ipairs(M.ORDER) do
        if name == S.kind then
            S.kind = M.ORDER[index % #M.ORDER + 1]
            break
        end
    end
    return M.KINDS[S.kind].label
end

--- The kind of the next parts.
---@pure
---@return string kind
function M.kind()
    return S.kind
end

--- Press at the cursor: start a part from the node there, or end the part being drawn there (a new node is made
--- when there is none). The result says what happened: `start`, `added`, `empty` (no node to start from), `same`,
--- `long` (longer than the kind allows), `twice` (the part exists) or `budget`.
---@param budget integer
---@return string result
function M.press(budget)
    local here = node_at(S.cx, S.cy)
    if S.drag == nil then
        if here == nil then return "empty" end
        S.drag = here
        return "start"
    end
    if here == S.drag then
        S.drag = nil
        return "same"
    end
    local from = S.nodes[S.drag]
    local kind = M.KINDS[S.kind]
    local span = math.sqrt((from.x - S.cx) ^ 2 + (from.y - S.cy) ^ 2)
    if span > kind.max + 0.01 then return "long" end
    if M.cost() + math.ceil(span * kind.cost) > budget then return "budget" end
    if here ~= nil then
        for _, part in ipairs(S.parts) do
            if (part.a == S.drag and part.b == here) or (part.a == here and part.b == S.drag) then return "twice" end
        end
    end
    local target = here or add_node(S.cx, S.cy)
    add_part(S.drag, target, S.kind)
    S.undo[#S.undo + 1] = { action = "add" }
    -- Go on drawing from the end of the part.
    S.drag = target
    return "added"
end

--- Stop drawing a part; true when one was being drawn.
---@return boolean stopped
function M.cancel()
    local stopped = S.drag ~= nil
    S.drag = nil
    return stopped
end

--- Whether a part is being drawn.
---@pure
---@return boolean drawing
function M.drawing()
    return S.drag ~= nil
end

local function remove_part(index)
    local part = table.remove(S.parts, index)
    if part.visual ~= 0 then scene.destroy_entity(part.visual) end
    return part
end

--- Remove the part nearest the cursor (within half a cell of its line).
---@return boolean removed
function M.remove_at_cursor()
    local best, best_distance = nil, 0.6
    for index, part in ipairs(S.parts) do
        local a, b = S.nodes[part.a], S.nodes[part.b]
        local dx, dy = b.x - a.x, b.y - a.y
        local t = math.max(0, math.min(1, ((S.cx - a.x) * dx + (S.cy - a.y) * dy) / (dx * dx + dy * dy)))
        local d = math.sqrt((a.x + dx * t - S.cx) ^ 2 + (a.y + dy * t - S.cy) ^ 2)
        if d < best_distance then best, best_distance = index, d end
    end
    if best == nil then return false end
    local part = remove_part(best)
    local a, b = drawn(part)
    S.undo[#S.undo + 1] = { action = "remove", a = { x = a.x, y = a.y }, b = { x = b.x, y = b.y }, kind = part.kind }
    S.drag = nil
    drop_loose_nodes()
    return true
end

--- Undo the last part added or removed.
---@return boolean undone
function M.undo()
    local last = table.remove(S.undo)
    if last == nil then return false end
    S.drag = nil
    if last.action == "add" then
        remove_part(#S.parts)
        drop_loose_nodes()
    else
        local a = node_at(last.a.x, last.a.y) or add_node(last.a.x, last.a.y)
        local b = node_at(last.b.x, last.b.y) or add_node(last.b.x, last.b.y)
        add_part(a, b, last.kind)
    end
    return true
end

--- Remove every part; the number removed.
---@return integer removed
function M.clear()
    local removed = #S.parts
    while #S.parts > 0 do remove_part(#S.parts) end
    S.undo, S.drag = {}, nil
    drop_loose_nodes()
    return removed
end

--- What the bridge costs.
---@pure
---@return integer cost
function M.cost()
    local total = 0
    for _, part in ipairs(S.parts) do
        total = total + math.ceil(length(part.a, part.b) * M.KINDS[part.kind].cost)
    end
    return total
end

--- Number of parts.
---@pure
---@return integer count
function M.count()
    return #S.parts
end

--- Draw the build aids: the cursor, and the part being drawn from its node to the cursor (red when too long).
---@param cursor entity
---@param preview entity
function M.draw(cursor, preview)
    transform.set_position(cursor, S.cx, S.cy, 0)
    ui.set_visible(preview, S.drag ~= nil and not S.running)
    if S.drag ~= nil then
        local from = S.nodes[S.drag]
        if from.x ~= S.cx or from.y ~= S.cy then
            stretch(preview, from.x, from.y, S.cx, S.cy, 1)
            local span = math.sqrt((from.x - S.cx) ^ 2 + (from.y - S.cy) ^ 2)
            if span > M.KINDS[S.kind].max + 0.01 then
                entity.set_tint(preview, 1, 0.25, 0.2, 0.8)
            else
                entity.set_tint(preview, 0.4, 1, 0.5, 0.8)
            end
        else
            ui.set_visible(preview, false)
        end
    end
end

-- The master part of a node: the first part touching it, which the others are hinged to.
local function masters()
    local out = {}
    for index, part in ipairs(S.parts) do
        if out[part.a] == nil then out[part.a] = index end
        if out[part.b] == nil then out[part.b] = index end
    end
    return out
end

-- The first entity of a tag not claimed yet: the part of the prefab just spawned.
local function unclaimed(tag, claimed, fallback)
    for _, id in ipairs(scene.find_entities(tag)) do
        if not claimed[id] then
            claimed[id] = true
            return id
        end
    end
    return fallback
end

--- Start the test run: each part becomes a body (`prefabs/<kind>.owlprefab`, a root and a `tail` part at its far
--- end), hinged at every node to the anchor there or to the first part of the node; the plan is hidden. A body lies
--- from the node its part was drawn from, so that the run holds the same whichever way the design is read; the
--- `Look` of a road body, its model, is turned back up when the part was drawn from right to left.
---@return integer bodies
function M.start_run()
    S.running, S.drag, S.broken = true, nil, 0
    local first = masters()
    local claimed = {}
    for _, part in ipairs(S.parts) do
        local a, b = S.nodes[part.a], S.nodes[part.b]
        part.body = scene.spawn_prefab(PREFABS[part.kind], 0, 0, 0)
        if part.back then
            stretch(part.body, b.x, b.y, a.x, a.y, 1, true)
        else
            stretch(part.body, a.x, a.y, b.x, b.y, 1, true)
        end
        part.tail = unclaimed("Tail", claimed, part.body)
        part.look = part.body
        if part.kind == "road" then
            part.look = unclaimed("Look", claimed, part.body)
            if part.back and part.look ~= part.body then transform.set_rotation(part.look, 0, 0, math.pi) end
        end
        ui.set_visible(part.visual, false)
    end
    for index, part in ipairs(S.parts) do
        local strength = M.KINDS[part.kind].strength
        -- The body holds the joint of the end the part was drawn from, the tail the other one.
        local from, to = part.a, part.b
        if part.back then from, to = part.b, part.a end
        for _, item in ipairs({ { from, part.body, -0.5 }, { to, part.tail, 0 } }) do
            local node_index, holder, local_x = item[1], item[2], item[3]
            local node = S.nodes[node_index]
            local other = nil
            if node.anchor ~= 0 then
                other = node.anchor
            elseif first[node_index] ~= index then
                other = S.parts[first[node_index]].body
            end
            if other ~= nil then physics.add_joint(holder, other, "hinge", local_x, 0, 0, strength) end
        end
    end
    for _, node in ipairs(S.nodes) do
        if node.anchor == 0 then ui.set_visible(node.visual, false) end
    end
    return #S.parts
end

--- End the test run: the bodies go, the plan comes back as it was; the number of parts.
---@return integer parts
function M.stop_run()
    S.running = false
    for _, part in ipairs(S.parts) do
        if part.body ~= 0 then scene.destroy_entity(part.body) end
        part.body, part.tail, part.look = 0, 0, 0
        ui.set_visible(part.visual, true)
    end
    for _, node in ipairs(S.nodes) do ui.set_visible(node.visual, true) end
    return #S.parts
end

--- Colour each part of the run by the load of its joints against their break force, green to red (white when
--- `show` is false); the highest ratio is returned.
---@param show boolean
---@return number stress
function M.stress(show)
    local top = 0
    for _, part in ipairs(S.parts) do
        if part.body ~= 0 then
            local strength = M.KINDS[part.kind].strength
            local load = math.max(physics.get_joint_load(part.body), physics.get_joint_load(part.tail))
            local ratio = math.min(1, load / strength)
            top = math.max(top, ratio)
            if show then
                entity.set_tint(part.look, math.min(1, ratio * 2), math.min(1, 2 - ratio * 2), 0.25, 1)
            else
                entity.set_tint(part.look, 1, 1, 1, 1)
            end
        end
    end
    return top
end

return M
