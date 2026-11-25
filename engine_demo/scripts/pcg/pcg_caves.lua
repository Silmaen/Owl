-- Caves of the procedural generation demo, on the entity of the `Tilemap`.
-- Demonstrates: `tilemap.generate` (a `.owlpcg` graph filling a tilemap layer from a seed, the same seed giving the same
--               cave), `tilemap.get_size` and `tilemap.get_cell` to count the walls, `ui.set_text` for the HUD and
--               `scene.transition_to` back to the hub.
--
-- The `pcg_regenerate` action draws the next seed. The gamestate mirrors the cave for the HUD and the scenario tests:
-- `pcg_seed` is the seed of the cave shown, `pcg_walls` its wall cells and `pcg_cells` its size in cells.

properties = {
    { name = "graph", type = "string", default = "pcg/caves.owlpcg" },
    { name = "label", type = "string", default = "CaveText" },
}

local label_id = 0
local seed = 7
local leaving = false

local function walls()
    local width, height = tilemap.get_size(entity_id)
    local count = 0
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            if tilemap.get_cell(entity_id, 0, x, y) == 1 then
                count = count + 1
            end
        end
    end
    return count, width * height
end

local function generate()
    local changed, reason = tilemap.generate(entity_id, graph, seed)
    if changed == nil then
        log.warn("Caves: " .. tostring(reason))
        return
    end
    local wall_count, cells = walls()
    gamestate.set("pcg_seed", seed)
    gamestate.set("pcg_walls", wall_count)
    gamestate.set("pcg_cells", cells)
    if label_id ~= 0 then
        ui.set_text(label_id, string.format("Seed %d    Walls %d / %d", seed, wall_count, cells))
    end
end

function on_create()
    label_id = scene.find_entity(label)
    generate()
end

function on_update(dt)
    if leaving then
        return
    end
    if input.is_action_just_pressed("pcg_regenerate") then
        seed = seed + 1
        generate()
    elseif input.is_action_just_pressed("pause") then
        leaving = true
        scene.transition_to("scenes/world_map.owl", "fade_out", 0.3)
    end
end
