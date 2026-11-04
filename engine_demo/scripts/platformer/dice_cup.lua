-- Throws the dice crates a few times on the fixed tick, from the scene seed: every Play gives the same throws, the
-- same landing and the same total (the deterministic simulation of the engine).
properties = {
    { name = "dice", type = "string", default = "DiceCrate1,DiceCrate2,DiceCrate3,DiceCrate4" },  -- tags of the crates
    { name = "throws", type = "int", default = 5 },  -- number of throws
    { name = "interval", type = "int", default = 30 },  -- ticks between two throws
    { name = "strength", type = "float", default = 3.5 },  -- impulse of a 6
}

local crates = {}
local ticks = 0
local thrown = 0

function on_create()
    crates = {}
    for tag in string.gmatch(dice, "[^,]+") do
        local id = scene.find_entity(tag)
        if id ~= 0 then crates[#crates + 1] = id end
    end
    gamestate.set("dice_total", 0)
end

function on_tick(dt)
    ticks = ticks + 1
    if thrown >= throws or ticks % interval ~= 0 then return end
    thrown = thrown + 1
    local total = 0
    for _, id in ipairs(crates) do
        local roll = math.random(6)
        total = total + roll
        local angle = math.pi / 2 + (math.random() - 0.5) * 0.8
        local push = strength * math.pow(roll / 6, 0.5)
        physics.impulse(id, math.cos(angle) * push, math.sin(angle) * push)
    end
    gamestate.set("dice_total", gamestate.get("dice_total") + total)
end
