-- Sound yard scene script: walk the ear (WASD) around the yard and listen. The cave rings (a Cave reverb zone), the
-- pool muffles everything (an Underwater zone with a low-pass filter), the radio plays behind a wall (a low-pass on the
-- source), the cart passes with a Doppler shift, and the adaptive music turns from calm to danger to combat as the
-- ear nears the beast, through the `yard_threat` game state key. M mutes the music bus; Escape goes back to the menu.
-- Demonstrates: ReverbZone, SoundSource filters and Doppler, AdaptiveMusic, gamestate keys, sound.set_bus_muted,
-- sound.get_music_state.

properties = {
    { name = "menu_scene", type = "string", default = "scenes/main_menu.owl" },
    { name = "speed", type = "float", default = 8.0 },
    { name = "danger_distance", type = "float", default = 14.0 },
    { name = "combat_distance", type = "float", default = 6.0 },
}

local ear_id = 0
local beast_id = 0
local readout_id = 0
local leaving = false

function on_create()
    ear_id = scene.find_entity("Ear")
    beast_id = scene.find_entity("Beast")
    readout_id = ui.find("YardReadout")
    gamestate.set("yard_threat", "calm")
end

local function threat_of(distance)
    if distance < combat_distance then
        return "combat"
    elseif distance < danger_distance then
        return "danger"
    end
    return "calm"
end

function on_update(delta)
    if leaving or ui.is_transition_active() then
        return
    end
    if input.is_action_just_pressed("pause") then
        leaving = true
        scene.transition_to(menu_scene, "fade_out", 0.3)
        return
    end
    if input.is_action_just_pressed("yard_mute_music") then
        sound.set_bus_muted("Music", not sound.is_bus_muted("Music"))
    end
    local dx, dy = input.get_action_vector("move")
    local x, y, z = transform.get_position(ear_id)
    x = x + dx * speed * delta
    y = y - dy * speed * delta
    transform.set_position(ear_id, x, y, z)
    local bx, by = transform.get_position(beast_id)
    local distance = math.sqrt((bx - x) ^ 2 + (by - y) ^ 2)
    gamestate.set("yard_threat", threat_of(distance))
    local muted = sound.is_bus_muted("Music") and " (muted)" or ""
    ui.set_text(readout_id, string.format(
        "WASD: walk    M: mute the music    Esc: menu\nMusic: %s%s\nLeft: cave reverb    Right: underwater    Up: radio behind a wall    Down: the cart (Doppler)",
        sound.get_music_state(), muted))
end
