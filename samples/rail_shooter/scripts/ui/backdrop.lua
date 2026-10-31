-- The live backdrop of the Ring Runner menus: the camera flies on at `speed`, the ship (when the scene has one)
-- cruises ahead of it, weaving a little, and every rock passed is brought back far ahead at another place, so the
-- ring never ends. Positions are read back each frame, so a shift of the floating origin changes nothing.

properties = {
    { name = "speed", type = "float", default = 14.0, min = 0.0 },
}

local camera_id, ship_id = 0, 0
local rocks = {}
local clock = 0.0
local seed = 1

local function next_random()
    seed = (seed * 1103515245 + 12345) % 2147483648
    return seed / 2147483648
end

function on_create()
    camera_id = scene.find_entity("MenuCamera")
    ship_id = scene.find_entity("MenuShip")
    for index, id in ipairs(scene.find_entities("MenuRock")) do
        local rx, ry, rz = transform.get_rotation(id)
        rocks[#rocks + 1] = { id = id, rx = rx, ry = ry, rz = rz, spin = 0.1 + (index % 5) * 0.06 }
    end
end

function on_update(dt)
    if camera_id == 0 then return end
    clock = clock + dt
    local cx, cy, cz = transform.get_position(camera_id)
    cz = cz - speed * dt
    transform.set_position(camera_id, cx, cy, cz)
    transform.set_rotation(camera_id, -0.12 + 0.02 * math.sin(clock * 0.3), 0.03 * math.sin(clock * 0.21), 0)
    if ship_id ~= 0 then
        local x = 2.6 + 0.9 * math.sin(clock * 0.5)
        local y = -1.4 + 0.4 * math.sin(clock * 0.8)
        transform.set_position(ship_id, cx + x, cy + y, cz - 9.0)
        transform.set_rotation(ship_id, 0.05 * math.cos(clock * 0.8), 0.25, -0.35 * math.cos(clock * 0.5))
    end
    for _, rock in ipairs(rocks) do
        local x, y, z = transform.get_position(rock.id)
        if z > cz + 8 then
            local side = next_random() < 0.5 and -1 or 1
            x = cx + side * (4 + next_random() * 36)
            y = cy - 14 + next_random() * 30
            z = z - 300
            transform.set_position(rock.id, x, y, z)
        end
        rock.ry = rock.ry + rock.spin * dt
        transform.set_rotation(rock.id, rock.rx, rock.ry, rock.rz)
    end
end
