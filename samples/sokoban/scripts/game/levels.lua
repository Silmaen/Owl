-- The levels of Sokoban, rows of the data table `tables/levels.owltable` in play order: the tilemap of each level and
-- its name. The title, the level select and the game read it (`require("scripts.game.levels")`); a level is the row of
-- its index, the next one plays after it and the victory screen after the last. One scene plays them all: the level to
-- play is the game state `sokoban_level`, and `open` shows its tilemap in the `Level` entity and frames it with the
-- camera. The progress is kept in the settings `sokoban_done_NN` and `sokoban_best_NN` (fewest moves), NN the index
-- on two digits.

local L = {}

L.TABLE = "tables/levels.owltable"
L.LEVEL = "scenes/level.owl"
L.VICTORY = "scenes/victory.owl"
L.CURRENT = "sokoban_level"

local keys = nil

local function rows()
    if keys == nil then keys = datatable.get_keys(L.TABLE) or {} end
    return keys
end

--- Number of levels.
---@return any result
function L.count()
    return #rows()
end

--- Tilemap of level `i`, nil past the last one.
---@param i any
---@return any result
function L.tilemap(i)
    local key = rows()[i]
    return key and datatable.get(L.TABLE, key, "tilemap") or nil
end

--- Scene to open to play level `i`, which becomes the level to play; nil past the last one.
---@param i any
---@return any result
function L.scene(i)
    if rows()[i] == nil then return nil end
    gamestate.set(L.CURRENT, i)
    return L.LEVEL
end

--- Show the level to play in the level scene and return its index: `fallback` when none was chosen (the scene played
--- on its own). The `Level` entity shows the tilemap, the camera and the background are sized to hold it.
---@param fallback any
---@return any result
function L.open(fallback)
    local i = gamestate.get(L.CURRENT, fallback)
    if rows()[i] == nil then i = fallback end
    local map = scene.find_entity("Level")
    tilemap.set_asset(map, L.tilemap(i))
    local width, height = tilemap.get_size(map)
    local size = math.max(height + 3, (width + 2) * 0.75)
    entity.set_camera_size(scene.find_entity("MainCamera"), size)
    transform.set_scale(scene.find_entity("Background"), size * 2.4, size * 2.4, 1)
    return i
end

--- Name of level `i` ("" when there is none).
---@param i any
---@return any result
function L.name(i)
    local key = rows()[i]
    return key and datatable.get(L.TABLE, key, "name") or ""
end

--- Scene played after level `i`: the next level, the victory screen after the last one.
---@param i any
---@return any result
function L.next_scene(i)
    return L.scene(i + 1) or L.VICTORY
end

---@param i any
---@return any result
function L.done_key(i)
    return string.format("sokoban_done_%02d", i)
end

---@param i any
---@return any result
function L.best_key(i)
    return string.format("sokoban_best_%02d", i)
end

--- Whether level `i` was solved once.
---@param i any
---@return any result
function L.done(i)
    return settings.get(L.done_key(i), false)
end

--- Fewest moves of level `i` (0 when not done).
---@param i any
---@return any result
function L.best(i)
    return settings.get(L.best_key(i), 0)
end

return L
