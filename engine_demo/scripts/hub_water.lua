-- Moving water of the hub: the river runs on four frames and the fountain's jet and ripples on two.
-- Demonstrates: `isometric.get_cell` / `isometric.set_cell` on a 2D tilemap (the layers `ground` = 0 and `objects` = 1),
--               a script editing tiles over time, the frame list of a tileset used as an animation.
--
-- The cells to animate are found once at creation by reading the ground tilemap over the river's rectangles (the same
-- ones as textures/generate_hub_village.py): every cell that holds a water tile keeps its place in the cycle, a
-- (column + row) offset so that the ripples do not all move together; the bridges are not water and stay as they are.

local RIVER = { { 0, 31, 25, 32 }, { 24, 29, 25, 32 }, { 24, 29, 44, 30 }, { 43, 29, 44, 32 }, { 43, 31, 63, 32 } }
local WATER = { 132, 133, 137, 138 }
local FOUNTAIN = { 31, 20 }
local FOUNTAIN_FRAMES = { 192, 196 }
local FRAME_TIME = 0.45

local map_id = 0
local cells = {}
local clock = 0
local shown_phase = -1

function on_create()
    map_id = scene.find_entity("WorldTilemap")
    local seen = {}
    for _, rect in ipairs(RIVER) do
        for cy = rect[2], rect[4] do
            for cx = rect[1], rect[3] do
                local id = cy * 100 + cx
                if not seen[id] then
                    seen[id] = true
                    local terrain = isometric.get_cell(map_id, 0, cx, cy)
                    if terrain == WATER[1] or terrain == WATER[2] or terrain == WATER[3] or terrain == WATER[4] then
                        cells[#cells + 1] = { cx, cy }
                    end
                end
            end
        end
    end
    gamestate.set("water_cells", #cells)
end

function on_update(dt)
    clock = clock + dt
    local phase = math.floor(clock / FRAME_TIME)
    if phase == shown_phase then
        return
    end
    shown_phase = phase
    for _, cell in ipairs(cells) do
        isometric.set_cell(map_id, 0, cell[1], cell[2], WATER[(phase + cell[1] // 2 + cell[2]) % 4 + 1], -1)
    end
    local first = FOUNTAIN_FRAMES[phase % 2 + 1]
    for k = 0, 3 do
        isometric.set_cell(map_id, 1, FOUNTAIN[1] + k % 2, FOUNTAIN[2] + k // 2, first + k, -1)
    end
    gamestate.set("water_phase", phase)
end
