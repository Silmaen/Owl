-- A barrel of the tavern corner: E while aiming at it shoves it away from the player, over the floor, into the stack.
-- Demonstrates: a 3D PhysicBody (Dimension Space, a cylinder), physics.impulse with a Z component, an Interaction
-- trigger moving with its body.

properties = {
    { name = "player", type = "string", default = "TavernCamera" },
    { name = "strength", type = "float", default = 0.9 },  -- impulse along the floor; the barrel weighs about 0.25
    { name = "lift", type = "float", default = 0.15 },     -- upward part, so it does not dig into the floor
}

-- Bound by Trigger.Type=Interaction with CallbackName=on_barrel_kick.
function on_barrel_kick()
    local player_id = scene.find_entity(player)
    if player_id == 0 then
        return
    end
    local px, _, pz = transform.get_position(player_id)
    local bx, _, bz = transform.get_position(entity_id)
    local dx, dz = bx - px, bz - pz
    local length = math.sqrt(dx * dx + dz * dz)
    if length < 1e-3 then
        return
    end
    physics.impulse(entity_id, dx / length * strength, lift, dz / length * strength)
end
