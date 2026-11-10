-- A rat of the cellar: its behaviour tree (`cellar_rat.owlbt`) calls `rat_bite` when it reaches the player.

function rat_bite(entity, dt)
	gamestate.set("cellar_bites", (gamestate.get("cellar_bites") or 0) + 1)
	return true
end
