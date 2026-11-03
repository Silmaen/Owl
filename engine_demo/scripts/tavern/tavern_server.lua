-- The server's round: the walk while its Spline Follower moves, the idle while a stop holds it (a table, the bar).
-- Demonstrates: the stops of a Spline Follower, read by spline.is_waiting, driving the Animator of the child mesh.

local body
local waiting = false

function on_create()
    body = scene.find_entity("Server")
end

function on_update(_)
    local now = spline.is_waiting(entity_id) or false
    if body and now ~= waiting then
        waiting = now
        animator.play(body, waiting and "idle" or "walk", 0.3)
    end
end
