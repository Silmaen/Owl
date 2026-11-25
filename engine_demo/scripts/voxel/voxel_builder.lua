-- Block building of the voxel terrain, on the entity of the `VoxelWorld`.
-- Demonstrates: the `voxel` table (`voxel.get_target`, `voxel.get_block`, `voxel.get_block_name`, `voxel.find_block`,
--               `voxel.set_place_block`), the `on_block_broken` / `on_block_placed` hooks of the `VoxelPlayer` edits
--               (mouse or gamepad triggers, with or without a captured cursor) and a save keeping the edited chunks of
--               a procedural world (`save.save_game` / `save.load_game` on `quick_save` / `quick_load`).
--
-- The `voxel_next_block` action walks the hotbar. The gamestate mirrors the building for the HUD and the scenario
-- tests: `voxel_broken` and `voxel_placed` count the edits, `voxel_hotbar` is the block the player places,
-- `voxel_last_x` / `_y` / `_z` the last edited block and `voxel_last_block` what the world holds there now.

properties = {
    { name = "player", type = "string", default = "TerrainPlayer" },
    { name = "label", type = "string", default = "BuildText" },
}

local HOTBAR = { "stone", "grass", "dirt", "sand", "snow" }
local SAVE_SLOT = 3
local player_id, label_id = 0, 0
local slot = 1

local function count(key)
    gamestate.set(key, gamestate.get(key, 0) + 1)
end

local function remember(x, y, z)
    gamestate.set("voxel_last_x", x)
    gamestate.set("voxel_last_y", y)
    gamestate.set("voxel_last_z", z)
end

local function choose(index)
    slot = index
    gamestate.set("voxel_hotbar", HOTBAR[slot])
    local block = voxel.find_block(entity_id, HOTBAR[slot])
    if block > 0 then
        voxel.set_place_block(player_id, block)
    end
end

function on_create()
    player_id = scene.find_entity(player)
    label_id = scene.find_entity(label)
    local kept = gamestate.get("voxel_hotbar", HOTBAR[1])
    local index = 1
    for i, name in ipairs(HOTBAR) do
        if name == kept then
            index = i
        end
    end
    choose(index)
end

function on_block_broken(x, y, z, block)
    count("voxel_broken")
    remember(x, y, z)
end

function on_block_placed(x, y, z, block)
    count("voxel_placed")
    remember(x, y, z)
end

function on_update(dt)
    if input.is_action_just_pressed("voxel_next_block") then
        choose(slot % #HOTBAR + 1)
    end
    if input.is_action_just_pressed("quick_save") then
        save.save_game(SAVE_SLOT)
    elseif input.is_action_just_pressed("quick_load") and save.has_save(SAVE_SLOT) then
        save.load_game(SAVE_SLOT)
    end
    if gamestate.get("voxel_last_y", -1000) > -1000 then
        local block = voxel.get_block(entity_id, gamestate.get("voxel_last_x", 0), gamestate.get("voxel_last_y", 0),
            gamestate.get("voxel_last_z", 0))
        gamestate.set("voxel_last_block", block)
    end
    if label_id == 0 then
        return
    end
    local aimed = "nothing"
    local x, y, z = voxel.get_target(player_id)
    if x ~= nil then
        aimed = voxel.get_block_name(entity_id, voxel.get_block(entity_id, x, y, z)) or "nothing"
    end
    ui.set_text(label_id, string.format("Aim: %s    Place: %s    Broken: %d    Placed: %d", aimed, HOTBAR[slot],
        gamestate.get("voxel_broken", 0), gamestate.get("voxel_placed", 0)))
end
