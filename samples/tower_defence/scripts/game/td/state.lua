-- The state of a level shared by the modules: gold, lives, waves, enemies, towers, the ring, the camera.

local td = require("scripts.game.td.core")
local S, C = td.state, td.data

S.sprite_lift = 0.5

S.bought = {}

S.foes_seen = {}

S.camera_id, S.cursor_id, S.ring_id, S.sky_id = 0, 0, 0, 0

S.choice_ids = {}

S.wheel_ids = { ring = 0, plaque = 0, title = 0, text = 0, costs = {} }

S.wheel_shown = {}      -- the text each world text of the wheel shows

S.banner_ids = {}       -- the waving banners: { id, yaw }

S.spells = {}           -- the spells the campaign unlocked: rain of arrows, shield wall

S.torch_ids = {}

S.hud = {}

S.shown = {}

S.stats = {}            -- tower stats by kind, enemy stats by kind, read from the properties

S.pads = {}             -- list of { id, x, y, z, level, tower }

S.pad_by_id = {}

S.route = nil           -- road A: the waypoints, with the distance walked at each one

S.routes = {}           -- road A, and road B of a level with two gates

S.enemies, S.towers, S.projectiles, S.bursts = {}, {}, {}, {}

S.corpses, S.sparks = {}, {}

S.gold, S.lives_left = 0, 0

S.wave, S.wave_count = 0, 0

S.countdown, S.spawning = 0.0, nil    -- seconds before the next wave; the queue of the wave being sent

S.spawn_clock = 0.0

S.spawned = 0

S.paused, S.fast = false, false

S.ending, S.end_clock = nil, 0.0

S.clock = 0.0

S.message_clock = 0.0

S.kills, S.leaks, S.built, S.earned, S.bonus_total, S.upgrades = 0, 0, 0, 0, 0, 0

S.travel, S.paused_travel, S.travel_at_pause = 0.0, 0.0, nil

S.last_shot_sound = {}

S.hours, S.hours_goal = 9.5, 9.5

S.night_lit = nil

S.ring = nil            -- { pad, choices = { {kind, id, locked}, ... } } while the ring is open

S.hover = 0             -- index of the highlighted choice

S.cursor_pad = nil

S.pad_mode = false

S.last_mouse_x, S.last_mouse_y = nil, nil

S.mouse_moved = false

S.view = { x = 9, z = 7, yaw = 0.0, yaw_goal = 0.0, distance = 15.0, eye = { 0.0, 0.0, 0.0 } }

S.basis = {}            -- camera axes of the frame: right, up, forward, toward

S.started = false

return td.state
