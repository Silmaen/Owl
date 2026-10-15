"""
Procedural isometric atlas generator (ISO3): terrain, slopes, foundations, overlays, sliced buildings and props.

Every image comes as three layers on the same grid: albedo, normal (world space, ``n * 0.5 + 0.5``) and depth
(height above the sprite's ground in world units, ``height / Z_RANGE * 255``). Run
``poetry run python -m tools.iso_atlas`` from the repository root.
"""
