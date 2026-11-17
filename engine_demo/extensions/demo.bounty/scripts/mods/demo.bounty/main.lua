-- Entry script of the demo.bounty mod: a Bounty component, given to an ammo box when the level loads, pays its gold
-- into the game state when the box is picked up.

components = {
    ["demo.bounty.Bounty"] = {
        { name = "gold", type = "int", default = 5, min = 0 },
    },
}

function on_scene_load()
    for _, box in ipairs(scene.find_entities("PickupAmmo1")) do
        entity.add_component(box, "demo.bounty.Bounty")
    end
end

function on_entity_destroy(id)
    local gold = entity.get_field(id, "demo.bounty.Bounty", "gold")
    if gold ~= nil then
        gamestate.set("mod_gold", (gamestate.get("mod_gold") or 0) + gold)
    end
end
