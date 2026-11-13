-- The levels of Sokoban, rows of the data table `tables/levels.owltable` in play order: the scene of each level and its
-- name. The title, the level select and the levels read it (`require("scripts.game.levels")`); a level is the row of
-- its index, the next one plays after it and the victory screen after the last. Its progress is kept in the settings
-- `sokoban_done_NN` and `sokoban_best_NN` (fewest moves), NN the index on two digits.

local L = {}

L.TABLE = "tables/levels.owltable"
L.VICTORY = "scenes/victory.owl"

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

--- Scene of level `i`, nil past the last one.
---@param i any
---@return any result
function L.scene(i)
    local key = rows()[i]
    return key and datatable.get(L.TABLE, key, "scene") or nil
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
