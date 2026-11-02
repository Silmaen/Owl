-- Scene script of the three mixed scenes: keys 1, 2 and 3 open the 3D yard, the raycast room and the voxel terrain.

local scenes = {
    [49] = "scenes/main.owl",
    [50] = "scenes/room.owl",
    [51] = "scenes/terrain.owl",
}

function on_update(dt)
    for key, path in pairs(scenes) do
        if input.is_key_just_pressed(key) then
            scene.load_scene(path)
            return
        end
    end
end
