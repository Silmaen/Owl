#!/usr/bin/env python3
"""Download the Poly Haven assets of the Engine Demo (CC0): the 1k equirectangular HDRIs into
``engine_demo/textures/hdri/``, the 1k PBR texture sets (base colour, OpenGL normal, packed AO / roughness /
metallic) into ``engine_demo/textures/pbr/<asset>/`` and the 1k glTF models (the ``.gltf``, its buffer and its
textures, at the relative paths the file names) into ``engine_demo/meshes/tavern/<asset>/``. Files already present
are kept."""
import json
import sys
import urllib.request
from pathlib import Path

DEMO = Path(__file__).resolve().parents[2] / "engine_demo"
ROOT = DEMO / "textures"
MODELS_ROOT = DEMO / "meshes" / "tavern"
API = "https://api.polyhaven.com/files/"
HEADERS = {"User-Agent": "OwlEngine-demo-fetch"}
RESOLUTION = "1k"
HDRIS = ["kloofendal_48d_partly_cloudy_puresky"]
TEXTURES = ["monastery_stone_floor", "plastered_stone_wall", "stacked_stone_wall", "rough_wood", "bark_brown_02",
            "rusty_metal_02", "burned_ground_01"]
MODELS = ["wooden_table_02", "folding_wooden_stool", "brass_pan_01", "brass_pot_01", "brass_pot_02", "brass_goblets",
          "carved_wooden_plate", "wooden_bowl_01", "yellow_onion", "brass_candleholders", "wooden_bucket_01",
          "wooden_bookshelf_worn"]


def fetch_json(url: str) -> dict:
    with urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS), timeout=60) as response:
        return json.load(response)


def download(url: str, target: Path) -> None:
    if target.exists():
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    print(f"{target.relative_to(DEMO)}")
    with urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS), timeout=120) as response:
        target.write_bytes(response.read())


def diffuse_key(files: dict) -> str:
    if "Diffuse" in files:
        return "Diffuse"
    return sorted(key for key in files if key.startswith("col"))[0]


def fetch_model(asset: str) -> None:
    """Download the 1k glTF of a model with every file it includes, keeping their relative paths."""
    gltf = fetch_json(API + asset)["gltf"][RESOLUTION]["gltf"]
    folder = MODELS_ROOT / asset
    for relative, file in gltf.get("include", {}).items():
        target = (folder / relative).resolve()
        if not target.is_relative_to(folder.resolve()):
            raise ValueError(f"{asset}: included file {relative} leaves the model folder")
        download(file["url"], target)
    download(gltf["url"], folder / f"{asset}.gltf")


def main() -> int:
    for asset in HDRIS:
        file = fetch_json(API + asset)["hdri"][RESOLUTION]["hdr"]
        download(file["url"], ROOT / "hdri" / f"{asset}.hdr")
    for asset in TEXTURES:
        files = fetch_json(API + asset)
        for key, name in ((diffuse_key(files), "diff"), ("nor_gl", "nor"), ("arm", "arm")):
            file = files[key][RESOLUTION]["jpg"]
            download(file["url"], ROOT / "pbr" / asset / f"{asset}_{name}.jpg")
    for asset in MODELS:
        fetch_model(asset)
    return 0


if __name__ == "__main__":
    sys.exit(main())
