-- The glade between the cottage and the troll bridge: the berry bush, the fireflies over the pond (a jar catches a
-- spark of them), the mushroom ring and the old oak in front, which the hero walks behind.
-- The rules shared by the rooms are in `scripts/game/adventure.lua`.

properties = {
    { name = "bridge_scene", type = "string", default = "scenes/bridge.owl" },
    { name = "cottage_scene", type = "string", default = "scenes/cottage.owl" },
    { name = "hotspots", type = "strings", default = {
        "fireflies use", "bush pick", "mushrooms look", "pond look", "to_cottage exit cottage_scene",
        "to_bridge exit bridge_scene" } },
}

local adventure = nil
local ids = {}
local clock = 0

local function refresh()
    ui.set_visible(ids.berries, not adventure.flag("berries_taken"))
end

function act(id, verb, item)
    if id == "bush" and verb ~= "look" then
        if item ~= nil then
            adventure.say("say.glade.bush_item")
        elseif adventure.flag("berries_taken") then
            adventure.say("say.glade.bush_empty")
        else
            adventure.act_animation(function()
                adventure.set_flag("berries_taken")
                adventure.give("berries")
                refresh()
                adventure.say("say.glade.berries")
            end)
        end
        return true
    end
    if id == "fireflies" and verb ~= "look" then
        if item == "jar" then
            adventure.act_animation(function()
                adventure.take("jar")
                adventure.give("firefly_jar")
                sound.play("sounds/fireflies.wav")
                ui.show_subtitle(tr("subtitle.fireflies"), 2)
                adventure.say("say.glade.fireflies_caught")
            end, 0.9)
        elseif item == "firefly_jar" then
            adventure.say("say.glade.fireflies_enough")
        elseif item ~= nil then
            adventure.say("say.glade.fireflies_item")
        else
            adventure.say("say.glade.fireflies_hands")
        end
        return true
    end
    if id == "mushrooms" and verb ~= "look" then
        adventure.say("say.glade.mushrooms_touch")
        return true
    end
    return false
end

function on_create()
    adventure = require("scripts.game.adventure")
    ids.berries = scene.find_entity("Prop_berries")
    ids.fireflies = scene.find_entity("Prop_fireflies")
    adventure.start({ name = "glade", hotspots = hotspots, act = act })
    refresh()
end

function on_update(dt)
    clock = clock + dt
    isometric.set_sprite_frame(ids.fireflies, math.floor(clock * 6) % 4)
    adventure.update(dt)
end
