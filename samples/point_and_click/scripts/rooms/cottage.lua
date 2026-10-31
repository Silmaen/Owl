-- Barnaby's cottage, where the game starts and ends: the jar on the shelf, the pie crust on the table, Barnaby by the
-- fire and the dark Moon Lantern. The moonstone set in the lantern and a spark of fireflies light it: the ending.
-- The rules shared by the rooms are in `scripts/game/adventure.lua`.

properties = {
    { name = "glade_scene", type = "string", default = "scenes/glade.owl" },
    { name = "hotspots", type = "strings", default = {
        "jar pick", "crust pick", "barnaby talk", "lantern use", "door exit glade_scene", "window look",
        "fireplace look", "shelf look", "table look" } },
    { name = "ending_scene", type = "string", default = "scenes/ending.owl" },
    { name = "ending_delay", type = "float", default = 2.5, min = 0 },
}

local adventure = nil
local ids = {}
local ending_timer = nil

local function refresh()
    ui.set_visible(ids.jar, not adventure.flag("jar_taken"))
    adventure.set_hotspot_enabled("jar", not adventure.flag("jar_taken"))
    ui.set_visible(ids.crust, not adventure.flag("crust_taken"))
    adventure.set_hotspot_enabled("crust", not adventure.flag("crust_taken"))
    isometric.set_sprite_frame(ids.lantern, adventure.flag("lantern_lit") and 1 or 0)
end

local function pick(flag, item, remark)
    adventure.act_animation(function()
        adventure.set_flag(flag)
        adventure.give(item)
        refresh()
        adventure.say(remark)
    end)
end

local function use_lantern(item)
    if item == "moonstone" then
        adventure.take("moonstone")
        adventure.set_flag("moonstone_set")
        adventure.say(adventure.flag("spark_set") and "say.cottage.lantern_stone_last" or "say.cottage.lantern_stone")
    elseif item == "firefly_jar" then
        adventure.take("firefly_jar")
        adventure.give("jar")
        adventure.set_flag("spark_set")
        adventure.say(adventure.flag("moonstone_set") and "say.cottage.lantern_spark_last" or "say.cottage.lantern_spark")
    elseif item ~= nil then
        adventure.say("say.cottage.lantern_item")
        return
    elseif adventure.flag("moonstone_set") then
        adventure.say("say.cottage.lantern_needs_spark")
        return
    else
        adventure.say("say.cottage.lantern_needs_stone")
        return
    end
    if adventure.flag("moonstone_set") and adventure.flag("spark_set") then
        adventure.set_flag("lantern_lit")
        refresh()
        sound.play("sounds/pickup.wav")
        ending_timer = ending_delay
        gamestate.set("lantern_lit", true)
    end
end

function act(id, verb, item)
    if id == "jar" and verb == "pick" then
        pick("jar_taken", "jar", "say.cottage.jar")
        return true
    end
    if id == "crust" and verb == "pick" then
        pick("crust_taken", "crust", "say.cottage.crust")
        return true
    end
    if id == "barnaby" and verb ~= "look" then
        if item ~= nil then
            adventure.say("say.cottage.barnaby_item")
        else
            animator.trigger(ids.barnaby, "talk")
            adventure.talk("dialogues/barnaby.owldlg")
        end
        return true
    end
    if id == "lantern" and verb ~= "look" then
        use_lantern(item)
        return true
    end
    return false
end

-- Conditions and events of Barnaby's dialogue.
function has_moonstone() return adventure.has("moonstone") or adventure.flag("moonstone_set") end
function has_spark() return adventure.has("firefly_jar") or adventure.flag("spark_set") end
function knows_troll() return adventure.flag("heard_troll") end
function on_heard_troll() adventure.set_flag("heard_troll") end

function on_create()
    adventure = require("scripts.game.adventure")
    ids.jar = scene.find_entity("Prop_jar")
    ids.crust = scene.find_entity("Prop_crust")
    ids.lantern = scene.find_entity("Prop_lantern")
    ids.barnaby = scene.find_entity("Barnaby")
    adventure.start({ name = "cottage", hotspots = hotspots, act = act })
    refresh()
    if gamestate.get("intro_done", false) == false then
        gamestate.set("intro_done", true)
        adventure.say("say.cottage.intro")
    end
end

function on_update(dt)
    adventure.update(dt)
    if ending_timer ~= nil then
        ending_timer = ending_timer - dt
        if ending_timer <= 0 then
            ending_timer = nil
            save.delete_save(1)
            scene.transition_to(ending_scene, "fade_out", 1.2, 0.85, 0.9, 1.0)
        end
    end
end
