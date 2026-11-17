-- Script nodes of the demo.nodes plugin: each annotated function is a node of the visual-script palette.
local M = {}

--- Value between two others, at a fraction of the way (0 gives the first, 1 the second).
---@param from number
---@param to number
---@param fraction number
---@return number value
---@pure
function M.blend(from, to, fraction)
    return from + (to - from) * fraction
end

--- Number of whole coins a score buys, at a price per coin.
---@param score integer
---@param price integer
---@return integer coins
---@pure
function M.coins_for(score, price)
    if price <= 0 then
        return 0
    end
    return score // price
end

return M
