-- The troll bridge: the troll sits on the moonstone and asks a toll of something sweet. The berry pie pays it: he eats
-- it, walks to the end of the bridge and leaves the stone. The rules shared by the rooms are in
-- `scripts/game/adventure.lua`.

properties = {
    { name = "glade_scene", type = "string", default = "scenes/glade.owl" },
    { name = "hotspots", type = "strings", default = {
        "moonstone pick", "troll talk", "sign look", "garden look", "stream look", "to_glade exit glade_scene" } },
    { name = "munch_time", type = "float", default = 1.6, min = 0 },
}

local adventure = nil
local ids = {}
local munching = nil
local wants_pie = false

-- Put the troll's hotspot where he stands.
local function follow_troll()
    local x, y = transform.get_world_position(ids.troll)
    local _, _, z = transform.get_position(ids.troll_spot)
    transform.set_position(ids.troll_spot, x + ids.spot_dx, y + ids.spot_dy, z)
end

local function refresh()
    local paid = adventure.flag("troll_paid")
    local taken = adventure.flag("moonstone_taken")
    ui.set_visible(ids.moonstone, paid and not taken)
    adventure.set_hotspot_enabled("moonstone", paid and not taken)
end

local function pay_toll()
    adventure.take("pie")
    sound.play("sounds/munch.wav")
    ui.show_subtitle(tr("subtitle.munch"), 2)
    animator.trigger(ids.troll, "talk")
    munching = munch_time
end

function act(id, verb, item)
    if id == "troll" and verb ~= "look" then
        if adventure.flag("troll_paid") then
            adventure.say("say.bridge.troll_full")
        elseif item == "pie" then
            pay_toll()
        elseif item == "berries" or item == "crust" then
            adventure.say("say.bridge.troll_half")
        elseif item ~= nil then
            adventure.say("say.bridge.troll_item")
        else
            adventure.talk("dialogues/troll.owldlg")
        end
        return true
    end
    if id == "moonstone" and verb ~= "look" then
        adventure.act_animation(function()
            adventure.set_flag("moonstone_taken")
            adventure.give("moonstone")
            refresh()
            adventure.say("say.bridge.moonstone")
        end)
        return true
    end
    return false
end

-- Condition and event of the troll's dialogue.
function has_pie() return adventure.has("pie") end
function on_troll_wants_pie() wants_pie = true end

function on_create()
    adventure = require("scripts.game.adventure")
    ids.moonstone = scene.find_entity("Prop_moonstone")
    ids.troll = scene.find_entity("Troll")
    ids.aside = scene.find_entity("TrollAside")
    ids.troll_spot = scene.find_entity("Hotspot_troll")
    local tx, ty = transform.get_world_position(ids.troll)
    local sx, sy = transform.get_world_position(ids.troll_spot)
    ids.spot_dx, ids.spot_dy = sx - tx, sy - ty
    adventure.start({ name = "bridge", hotspots = hotspots, act = act })
    if adventure.flag("troll_paid") then
        local x, y = transform.get_world_position(ids.aside)
        local _, _, z = transform.get_position(ids.troll)
        transform.set_position(ids.troll, x, y, z)
    end
    follow_troll()
    refresh()
end

function on_update(dt)
    adventure.update(dt)
    if adventure.flag("troll_paid") then follow_troll() end
    if wants_pie and not adventure.is_talking() then
        wants_pie = false
        pay_toll()
    end
    if munching ~= nil then
        munching = munching - dt
        if munching <= 0 then
            munching = nil
            adventure.set_flag("troll_paid")
            adventure.say("say.bridge.troll_paid")
            local x, y = transform.get_world_position(ids.aside)
            walk.go_to(ids.troll, x, y)
            refresh()
        end
    end
end
