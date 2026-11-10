-- Salvage tug of the far orbit, moored to a buoy by the station: an articulated body. The tug is one rigid assembly of
-- three entities, its hull with a booster and a solar panel merged as parts (`PhysicBody` part of the parent body): the
-- assembly weighs the three, its centre of mass sits between them. The booster pushes at its own place on the tug
-- (`physics.add_force` on a part), off the centre of mass, so the tug turns as it pulls. The thrust grows until the
-- mooring, a distance joint with a break force, snaps (`on_joint_break`); `stage_delay` seconds later the booster is
-- spent and breaks off (`physics.detach`): it flies on with a body of its own and the lighter tug coasts.
-- Demonstrates: physics.add_force, physics.detach, physics.get_mass, physics.get_assembly, on_joint_break, gamestate.

properties = {
    { name = "booster", type = "string", default = "TugBooster" },
    { name = "thrust", type = "float", default = 4.0, min = 0 },      -- newtons at the start
    { name = "thrust_ramp", type = "float", default = 6.0, min = 0 }, -- newtons gained per second
    { name = "stage_delay", type = "float", default = 2.0, min = 0 },
}

local booster_id = 0
local force = 0
local snapped_for = -1

function on_create()
    booster_id = scene.find_entity(booster)
    force = thrust
    gamestate.set("orbit_tether_snapped", 0)
    gamestate.set("orbit_stage_separated", 0)
    gamestate.set("orbit_tug_mass", physics.get_mass(entity_id))
end

-- A steady force goes in on_tick: one push per simulation step, whatever the frame rate.
function on_tick(dt)
    if booster_id == 0 or physics.get_assembly(booster_id) == nil then
        return
    end
    physics.add_force(booster_id, force, 0, 0)
    force = force + thrust_ramp * dt
    if snapped_for < 0 then
        return
    end
    snapped_for = snapped_for + dt
    if snapped_for >= stage_delay then
        physics.detach(booster_id)
        gamestate.set("orbit_stage_separated", 1)
        gamestate.set("orbit_tug_mass", physics.get_mass(entity_id))
    end
end

function on_joint_break(other_id)
    snapped_for = 0
    gamestate.set("orbit_tether_snapped", 1)
end
