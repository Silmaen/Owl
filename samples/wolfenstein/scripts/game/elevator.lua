-- The elevator ride at the end of a Wolfenstein mini floor: the script of the `ElevatorRide` entity, whose
-- `SequencePlayer` plays `sequences/elevator.owlseq` through the `ElevatorCamera`.
--
-- exit.lua closes the floor and sets `wolf_ride` to 1. The ride then starts where the player stands, turned as the
-- player (the keys of the sequence are in the space of this entity): the view turns round to the elevator door, the
-- door closes, the elevator hums up and the screen fades into the tally. Interact, accept or pause skips it, straight
-- to the tally. With the `reduced_motion` setting the view does not turn: the ride starts at the end of the turn.
-- `wolf_ride` is 2 while the ride plays and 3 once the tally is on its way.

properties = {
    { name = "tally_scene", type = "string", default = "scenes/tally.owl" },
    { name = "fade_seconds", type = "float", default = 0.6, min = 0 },
    { name = "turn_seconds", type = "float", default = 1.2, min = 0 },  -- time of the sequence at the end of the turn
    { name = "skip_after", type = "float", default = 0.3, min = 0 },    -- seconds before a press skips the ride
    { name = "fade_color", type = "color", default = { 0.22, 0.02, 0.02, 1 } },
}

local player_id, door_id = 0, 0
local riding = false
local leaving = false
local ride_time = 0

function on_create()
    player_id = scene.find_entity("Player")
    door_id = scene.find_entity("ElevatorDoor")
    gamestate.set("wolf_ride", 0)
end

local function leave()
    if leaving then
        return
    end
    leaving = true
    gamestate.set("wolf_ride", 3)
    scene.transition_to(tally_scene, "fade_out", fade_seconds, fade_color.r, fade_color.g, fade_color.b)
end

-- Event key of the sequence, once the view faces the door: the door closes behind the player.
function on_ride_turned()
    if door_id ~= 0 and door.get_state(door_id) ~= "idle" then
        door.close(door_id)
    end
end

-- Event key of the sequence: the screen fades into the tally while the elevator climbs.
function on_ride_fade()
    leave()
end

-- `onFinished` of the player: the end of the sequence, or a skip.
function on_ride_finished()
    leave()
end

local function start()
    riding = true
    ride_time = 0
    gamestate.set("wolf_ride", 2)
    if player_id ~= 0 then
        local x, y, z = transform.get_position(player_id)
        local _, _, yaw = transform.get_rotation(player_id)
        transform.set_position(entity_id, x, y, z)
        transform.set_rotation(entity_id, 0, 0, yaw)
    end
    sound.play("sounds/game/elevator.wav")
    ui.show_subtitle("[The elevator hums and climbs]", 2.5)
    local reduced = settings.get("reduced_motion", false)
    if not sequence.play(entity_id, reduced and turn_seconds or 0) then
        leave()
    elseif reduced then
        on_ride_turned()
    end
end

function on_update(dt)
    if leaving then
        return
    end
    if not riding then
        if gamestate.get("wolf_ride", 0) == 1 then
            start()
        end
        return
    end
    ride_time = ride_time + dt
    if ride_time >= skip_after and (input.is_action_just_pressed("interact") or input.is_action_just_pressed("ui_accept")
            or input.is_action_just_pressed("pause")) then
        gamestate.set("wolf_ride_skipped", 1)
        if not sequence.stop(entity_id) then
            leave()
        end
    end
end
