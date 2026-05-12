#!/usr/bin/env python3
"""Build a hi-res Wolfenstein 3D atlas from extracted PNGs.

Workflow:

  1. Optional: extract walls + sprites from a ``VSWAP.WL6`` (or ``.WL1``
     shareware) into the source directory. Requires you to own a copy of
     Wolfenstein 3D — the script does not bundle game data.
  2. Upscale each PNG in the source directory. Real-ESRGAN is auto-detected
     when ``realesrgan-ncnn-vulkan`` is on ``PATH`` (best quality for pixel
     art); alpha is preserved by upscaling RGB and the alpha mask separately.
  3. Compose either the original 4x4 atlas (matching ``raycast_walls.owltileset``)
     or a wide atlas of every wall, and write per-sprite PNGs into an output
     directory.

Defaults read from ``sample_project/textures/raycast_refs/`` and write to
``sample_project/textures/raycast_walls_hd.png`` + ``raycast_refs_hd/``.

Usage::

    poetry run python tools/build_wolf3d_atlas.py                          # use existing PNGs
    poetry run python tools/build_wolf3d_atlas.py --vswap path/VSWAP.WL6   # extract first
    poetry run python tools/build_wolf3d_atlas.py --upscaler lanczos       # force Pillow
    poetry run python tools/build_wolf3d_atlas.py --tile-size 256          # bigger output
    poetry run python tools/build_wolf3d_atlas.py --list                   # dry run
    poetry run python tools/build_wolf3d_atlas.py \\
        --source /tmp/wolf3d_dump \\
        --no-atlas \\
        --all-walls-atlas sample_project/textures/wolf3d_walls_full.png \\
        --sprite-range 2-49 \\
        --out-sprites sample_project/textures/wolf3d_sprites

Copyright note: the original Wolf3D assets are owned by id Software /
Bethesda. Extracting them for personal use from a copy you own is fine;
redistributing the upscaled outputs is not — keep them out of any public
artifact you publish.
"""

from __future__ import annotations

import argparse
import shutil
import struct
import subprocess
from pathlib import Path

from PIL import Image

# ---------------------------------------------------------------------------
# Paths & defaults
# ---------------------------------------------------------------------------
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
DEFAULT_SOURCE = PROJECT_ROOT / "sample_project" / "textures" / "raycast_refs"
DEFAULT_OUT_ATLAS = PROJECT_ROOT / "sample_project" / "textures" / "raycast_walls_hd.png"
DEFAULT_OUT_SPRITES = PROJECT_ROOT / "sample_project" / "textures" / "raycast_refs_hd"
DEFAULT_TILE_SIZE = 128

WALL_MAPPING: list[tuple[int, str]] = [
    (0, "doorpattern"),
    (1, "greystone"),
    (2, "bluestone"),
    (3, "wood"),
    (4, ""),  # was "barrel" — sprite slot
    (5, "brickpattern"),
    (6, "colorstone"),
    (7, ""),  # was "eagle" — Nazi imagery
    (8, ""),  # was "greenlight" — sprite slot
    (9, "mossy"),
    (10, ""),  # was "pillar" — sprite slot
    (11, "purplestone"),
    (12, "redbrick"),
]

ATLAS_COLS = 4
ATLAS_ROWS = 4

# ---------------------------------------------------------------------------
# Wolf3D VGA palette — canonical 256-colour table extracted verbatim from the
# SLADE editor's "Wolfenstein 3D.pal" (a byte-exact dump of GAMEPAL.OBJ
# shipped with the id Software 1992 release). The original VGA values were
# 6-bit (0..63) but the on-disk file stores them pre-scaled to 0..255.
# ---------------------------------------------------------------------------
_WOLF3D_PALETTE_HEX = (
    "000000" "0000a8" "00a800" "00a8a8" "a80000" "a800a8" "a85400" "a8a8a8"
    "545454" "5454fc" "54fc54" "54fcfc" "fc5454" "fc54fc" "fcfc54" "fcfcfc"
    "ececec" "dcdcdc" "d0d0d0" "c0c0c0" "b4b4b4" "a8a8a8" "989898" "8c8c8c"
    "7c7c7c" "707070" "646464" "545454" "484848" "383838" "2c2c2c" "202020"
    "fc0000" "ec0000" "e00000" "d40000" "c80000" "bc0000" "b00000" "a40000"
    "980000" "880000" "7c0000" "700000" "640000" "580000" "4c0000" "400000"
    "fcd8d8" "fcb8b8" "fc9c9c" "fc7c7c" "fc5c5c" "fc4040" "fc2020" "fc0000"
    "fca85c" "fc9840" "fc8820" "fc7800" "e46c00" "cc6000" "b45400" "9c4c00"
    "fcfcd8" "fcfcb8" "fcfc9c" "fcfc7c" "fcf85c" "fcf440" "fcf420" "fcf400"
    "e4d800" "ccc400" "b4ac00" "9c9c00" "848400" "706c00" "585400" "404000"
    "d0fc5c" "c4fc40" "b4fc20" "a0fc00" "90e400" "80cc00" "74b400" "609c00"
    "d8fcd8" "bcfcb8" "9cfc9c" "80fc7c" "60fc5c" "40fc40" "20fc20" "00fc00"
    "00fc00" "00ec00" "00e000" "00d400" "04c800" "04bc00" "04b000" "04a400"
    "049800" "048800" "047c00" "047000" "046400" "045800" "044c00" "044000"
    "d8fcfc" "b8fcfc" "9cfcfc" "7cfcf8" "5cfcfc" "40fcfc" "20fcfc" "00fcfc"
    "00e4e4" "00cccc" "00b4b4" "009c9c" "008484" "007070" "005858" "004040"
    "5cbcfc" "40b0fc" "20a8fc" "009cfc" "008ce4" "007ccc" "006cb4" "005c9c"
    "d8d8fc" "b8bcfc" "9c9cfc" "7c80fc" "5c60fc" "4040fc" "2024fc" "0004fc"
    "0000fc" "0000ec" "0000e0" "0000d4" "0000c8" "0000bc" "0000b0" "0000a4"
    "000098" "000088" "00007c" "000070" "000064" "000058" "00004c" "000040"
    "282828" "fce034" "fcd424" "fccc18" "fcc008" "fcb400" "b420fc" "a800fc"
    "9800e4" "8000cc" "7400b4" "60009c" "500084" "440070" "340058" "280040"
    "fcd8fc" "fcb8fc" "fc9cfc" "fc7cfc" "fc5cfc" "fc40fc" "fc20fc" "fc00fc"
    "e000e4" "c800cc" "b400b4" "9c009c" "840084" "6c0070" "580058" "400040"
    "fce8dc" "fce0d0" "fcd8c4" "fcd4bc" "fcccb0" "fcc4a4" "fcbc9c" "fcb890"
    "fcb080" "fca470" "fc9c60" "f0945c" "e88c58" "dc8854" "d08050" "c87c4c"
    "bc7848" "b47044" "a86840" "a0643c" "9c6038" "905c34" "885830" "80502c"
    "744c28" "6c4824" "5c4020" "543c1c" "483818" "403018" "382c14" "28200c"
    "600064" "006464" "006060" "00001c" "00002c" "302410" "480048" "500050"
    "000034" "1c1c1c" "4c4c4c" "5c5c5c" "404040" "303030" "343434" "d8f4f4"
    "b8e8e8" "9cdcdc" "74c8c8" "48c0c0" "20b4b4" "20b0b0" "00a4a4" "009898"
    "008c8c" "008484" "007c7c" "007878" "007474" "007070" "006c6c" "980088"
)
WOLF3D_PALETTE: list[tuple[int, int, int]] = [
    (
        int(_WOLF3D_PALETTE_HEX[i : i + 2], 16),
        int(_WOLF3D_PALETTE_HEX[i + 2 : i + 4], 16),
        int(_WOLF3D_PALETTE_HEX[i + 4 : i + 6], 16),
    )
    for i in range(0, len(_WOLF3D_PALETTE_HEX), 6)
]
assert len(WOLF3D_PALETTE) == 256, f"palette length is {len(WOLF3D_PALETTE)}"


# ---------------------------------------------------------------------------
# VSWAP wall + sprite extractor
# ---------------------------------------------------------------------------
def _decode_wall_chunk(chunk: bytes) -> Image.Image:
    """Decode a 4096-byte Wolf3D wall chunk into a 64x64 RGB image.

    Walls are stored column-major: byte ``col * 64 + row`` is the palette
    index for pixel ``(col, row)``.
    """
    img = Image.new("RGB", (64, 64))
    px = img.load()
    for col in range(64):
        for row in range(64):
            px[col, row] = WOLF3D_PALETTE[chunk[col * 64 + row]]
    return img


def _decode_sprite_chunk(chunk: bytes) -> Image.Image:
    """Decode a Wolf3D compiled-sprite chunk into a 64x64 RGBA image.

    Wolf3D sprite layout::

      uint16 leftpix
      uint16 rightpix
      uint16 column_offsets[rightpix - leftpix + 1]   # bytes from chunk start

    For each column the offset points to a list of "post" triplets
    terminated by ``endY == 0``::

      uint16 endY            (exclusive end row, pre-multiplied by 2)
      int16  pixelDataOffset (signed bytes from chunk start)
      uint16 startY          (start row, pre-multiplied by 2)

    The original 16-bit DOS build relied on pointer arithmetic wrapping
    modulo 65536, so ``pixelDataOffset`` is often negative. We replicate
    that with ``(off + y) & 0xFFFF``.
    """
    img = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    if len(chunk) < 4:
        return img
    px = img.load()
    leftpix, rightpix = struct.unpack_from("<HH", chunk, 0)
    if leftpix > rightpix or rightpix >= 64 or leftpix >= 64:
        return img
    num_cols = rightpix - leftpix + 1
    cols_table_end = 4 + num_cols * 2
    if cols_table_end > len(chunk):
        return img
    column_offsets = struct.unpack_from(f"<{num_cols}H", chunk, 4)
    for col_idx, col in enumerate(range(leftpix, rightpix + 1)):
        cmd = column_offsets[col_idx]
        while cmd + 2 <= len(chunk):
            end_y = struct.unpack_from("<H", chunk, cmd)[0]
            if end_y == 0:
                break
            if cmd + 6 > len(chunk):
                break
            _, pix_offset, start_y = struct.unpack_from("<HhH", chunk, cmd)
            cmd += 6
            for y in range(start_y // 2, end_y // 2):
                if y < 0 or y >= 64:
                    continue
                addr = (pix_offset + y) & 0xFFFF
                if 0 <= addr < len(chunk):
                    rgb = WOLF3D_PALETTE[chunk[addr]]
                    px[col, y] = (rgb[0], rgb[1], rgb[2], 255)
    return img


def extract_vswap(vswap_path: Path, out_dir: Path, walls: bool, sprites: bool) -> tuple[int, int]:
    """Dump walls (and optionally sprites) from a Wolf3D VSWAP file."""
    data = vswap_path.read_bytes()
    chunk_count, sprite_start, sound_start = struct.unpack_from("<HHH", data, 0)
    offsets_base = 6
    lengths_base = offsets_base + chunk_count * 4
    out_dir.mkdir(parents=True, exist_ok=True)

    walls_written = 0
    if walls:
        for i in range(sprite_start):
            offset = struct.unpack_from("<I", data, offsets_base + i * 4)[0]
            length = struct.unpack_from("<H", data, lengths_base + i * 2)[0]
            if length != 4096:
                print(f"  wall {i}: unexpected length {length}, skipping")
                continue
            chunk = data[offset : offset + length]
            _decode_wall_chunk(chunk).save(out_dir / f"wall_{i:03d}.png", "PNG")
            walls_written += 1

    sprites_written = 0
    if sprites:
        for s_idx, chunk_idx in enumerate(range(sprite_start, sound_start)):
            offset = struct.unpack_from("<I", data, offsets_base + chunk_idx * 4)[0]
            length = struct.unpack_from("<H", data, lengths_base + chunk_idx * 2)[0]
            if offset == 0 or length == 0:
                continue
            chunk = data[offset : offset + length]
            _decode_sprite_chunk(chunk).save(out_dir / f"sprite_{s_idx:03d}.png", "PNG")
            sprites_written += 1

    try:
        location = str(out_dir.relative_to(PROJECT_ROOT))
    except ValueError:
        location = str(out_dir)
    msg = []
    if walls:
        msg.append(f"{walls_written} walls")
    if sprites:
        msg.append(f"{sprites_written} sprites")
    print(f"  extracted {' + '.join(msg)} to {location}")
    return walls_written, sprites_written


# ---------------------------------------------------------------------------
# Upscalers
# ---------------------------------------------------------------------------
def detect_realesrgan() -> str | None:
    """Return the binary name for Real-ESRGAN if it's on PATH, else None."""
    for candidate in ("realesrgan-ncnn-vulkan", "realesrgan"):
        if shutil.which(candidate):
            return candidate
    return None


def _image_max_channel(img: Image.Image) -> int:
    """Return the brightest single-channel value across all pixels of ``img``."""
    rgb = img.convert("RGB")
    return max(rgb.getextrema(), key=lambda t: t[1])[1]


def _run_realesrgan_with_retry(src: Path, dst: Path, scale: int, binary: str,
                                label: str | None = None, max_attempts: int = 3) -> None:
    """Invoke Real-ESRGAN and verify the output is not pathologically black.

    Real-ESRGAN ncnn-vulkan occasionally returns an all-black image without
    failing — typically when the GPU pipeline is reused too aggressively
    across sequential invocations on very dark inputs. We probe the input
    luminance: if the source has any non-trivially-bright pixels but the
    output comes back as essentially pure black, we retry up to
    ``max_attempts`` times before falling back to Lanczos.
    """
    src_label = label or src.name
    src_max = _image_max_channel(Image.open(src))
    for attempt in range(1, max_attempts + 1):
        cmd = [
            binary, "-i", str(src), "-o", str(dst), "-s", str(scale), "-n",
            "realesrgan-x4plus-anime" if scale == 4 else "realesr-animevideov3",
        ]
        result = subprocess.run(cmd, capture_output=True)
        if result.returncode != 0:
            err = result.stderr.decode(errors="replace").strip()
            print(f"    WARN realesrgan failed for {src_label} (attempt {attempt}, rc={result.returncode}): {err}")
            continue
        if not dst.exists() or dst.stat().st_size == 0:
            print(f"    WARN realesrgan produced no output for {src_label} (attempt {attempt})")
            continue
        dst_max = _image_max_channel(Image.open(dst))
        if src_max > 8 and dst_max <= 4:
            print(f"    WARN realesrgan returned black for {src_label} "
                  f"(src_max={src_max}, dst_max={dst_max}, attempt {attempt}/{max_attempts})")
            continue
        return
    print(f"    WARN realesrgan giving up on {src_label} after {max_attempts} attempts, falling back to lanczos")
    src_img = Image.open(src).convert("RGB")
    src_w, src_h = src_img.size
    src_img.resize((src_w * scale, src_h * scale), Image.LANCZOS).save(dst, "PNG")


def upscale_with_realesrgan(src: Path, dst: Path, scale: int, binary: str,
                             label: str | None = None) -> None:
    """Run Real-ESRGAN preserving the alpha channel.

    Real-ESRGAN ncnn-vulkan silently drops or scrambles RGBA inputs. We split
    RGB and alpha: RGB goes through the upscaler on a temp file (background
    filled with the average opaque colour to keep edges smooth), the alpha
    mask is re-attached at the upscaled resolution via NEAREST (Wolf3D
    sprites have binary alpha).
    """
    src_img = Image.open(src).convert("RGBA")
    has_alpha = src_img.getchannel("A").getextrema()[0] < 255
    if not has_alpha:
        _run_realesrgan_with_retry(src, dst, scale, binary, label=label)
        return

    # Bleed opaque RGB outward into the transparent region so Real-ESRGAN
    # sees a continuous colour gradient at the silhouette boundary rather
    # than a hard cut to "background grey". Without this, the upscaler
    # invents a halo of mid-grey pixels along the alpha edge.
    rgb_only = _bleed_alpha_edges(src_img, iterations=8)

    tmp_rgb_in = src.with_suffix(".rgb_only.png")
    rgb_only.save(tmp_rgb_in, "PNG")
    try:
        _run_realesrgan_with_retry(tmp_rgb_in, dst, scale, binary, label=label or src.name)
    finally:
        tmp_rgb_in.unlink(missing_ok=True)

    up_rgb = Image.open(dst).convert("RGB")
    up_alpha = src_img.getchannel("A").resize(up_rgb.size, Image.NEAREST)
    out = Image.merge("RGBA", (*up_rgb.split(), up_alpha))
    out.save(dst, "PNG")


def _tile_wrap_3x3(img: Image.Image) -> Image.Image:
    """Tile ``img`` 3x3 so edge-aware upscalers see a seamless context."""
    w, h = img.size
    tiled = Image.new(img.mode, (3 * w, 3 * h))
    for dx in range(3):
        for dy in range(3):
            tiled.paste(img, (dx * w, dy * h))
    return tiled


def _bleed_alpha_edges(rgba: Image.Image, iterations: int = 8) -> Image.Image:
    """Extend opaque RGB outward into transparent neighbours.

    Real-ESRGAN doesn't understand alpha: when it processes the boundary
    between opaque silhouette and transparent background it pulls the
    background colour into the edge, leaving a grey halo. We pre-fill the
    transparent region with the average colour of adjacent opaque pixels so
    the upscaler sees a smooth gradient at the silhouette edge and produces
    clean RGB. The original alpha mask is then re-applied on top of the
    upscaled output to keep the silhouette crisp.
    """
    rgba = rgba.convert("RGBA")
    w, h = rgba.size
    px = rgba.load()
    rgb = [[(px[x, y][0], px[x, y][1], px[x, y][2]) for x in range(w)] for y in range(h)]
    alpha = [[px[x, y][3] > 0 for x in range(w)] for y in range(h)]
    for _ in range(iterations):
        # Snapshot so we extend uniformly per iteration.
        new_alpha = [row[:] for row in alpha]
        new_rgb = [row[:] for row in rgb]
        for y in range(h):
            for x in range(w):
                if alpha[y][x]:
                    continue
                r_sum = g_sum = b_sum = 0
                cnt = 0
                for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h and alpha[ny][nx]:
                        r_sum += rgb[ny][nx][0]
                        g_sum += rgb[ny][nx][1]
                        b_sum += rgb[ny][nx][2]
                        cnt += 1
                if cnt > 0:
                    new_rgb[y][x] = (r_sum // cnt, g_sum // cnt, b_sum // cnt)
                    new_alpha[y][x] = True
        rgb = new_rgb
        alpha = new_alpha
    out = Image.new("RGB", (w, h))
    out.putdata([rgb[y][x] for y in range(h) for x in range(w)])
    return out


def upscale_image(src: Path, target: int, upscaler: str, tileable: bool = False) -> Image.Image:
    """Return a ``target × target`` RGBA image, upscaled from ``src``.

    When ``tileable`` is true the source is wrapped 3x3 before upscale so that
    edge-aware models (Real-ESRGAN) see continuous patterns at the borders.
    The central tile is then cropped out, which preserves the cyclic property
    of the input — adjacent copies of the upscaled output still join cleanly.
    """
    img = Image.open(src).convert("RGBA")
    if img.size == (target, target):
        return img
    if max(img.size) >= target:
        return img.resize((target, target), Image.LANCZOS)
    if upscaler == "realesrgan":
        binary = detect_realesrgan()
        if binary is None:
            print(f"    WARN realesrgan not on PATH, falling back to lanczos for {src.name}")
            return img.resize((target, target), Image.LANCZOS)
        # Choose the scale factor that brings the (possibly wrapped) source to
        # at least target size.
        if tileable:
            # Tile the source 3x3 in RGB (Real-ESRGAN's RGBA path scrambles the
            # alpha channel even when source alpha is uniformly 255, leaving the
            # whole output transparent), upscale, then crop the central tile.
            tiled = _tile_wrap_3x3(img.convert("RGB"))
            scale = min(s for s in (2, 3, 4) if max(img.size) * s >= target)
            tmp_in = src.with_suffix(".tiled.png")
            tmp_out = src.with_suffix(".upscaled.png")
            tiled.save(tmp_in, "PNG")
            try:
                upscale_with_realesrgan(tmp_in, tmp_out, scale, binary, label=f"{src.name} [tiled]")
                up = Image.open(tmp_out).convert("RGBA")
                up_w, up_h = up.size
                center_w = up_w // 3
                center_h = up_h // 3
                up = up.crop((center_w, center_h, 2 * center_w, 2 * center_h))
                return up.resize((target, target), Image.LANCZOS)
            finally:
                tmp_in.unlink(missing_ok=True)
                tmp_out.unlink(missing_ok=True)
        scale = min(s for s in (2, 3, 4) if max(img.size) * s >= target)
        tmp_out = src.with_suffix(".upscaled.png")
        try:
            upscale_with_realesrgan(src, tmp_out, scale, binary, label=src.name)
            up = Image.open(tmp_out).convert("RGBA")
            return up.resize((target, target), Image.LANCZOS)
        finally:
            tmp_out.unlink(missing_ok=True)
    if upscaler == "nearest":
        return img.resize((target, target), Image.NEAREST)
    return img.resize((target, target), Image.LANCZOS)


# ---------------------------------------------------------------------------
# Atlas + sprite composition
# ---------------------------------------------------------------------------
def compose_atlas(source_dir: Path, tile_size: int, upscaler: str, out_atlas: Path) -> None:
    """Compose the historic 4x4 atlas using ``WALL_MAPPING`` filename stems."""
    atlas = Image.new("RGBA", (ATLAS_COLS * tile_size, ATLAS_ROWS * tile_size), (0, 0, 0, 0))
    print("  atlas slots (tileable):")
    for idx, stem in WALL_MAPPING:
        if not stem:
            print(f"    [{idx:2d}] (empty)")
            continue
        candidate = source_dir / f"{stem}.png"
        if not candidate.exists():
            print(f"    [{idx:2d}] {stem:<14} — MISSING ({candidate})")
            continue
        tile = upscale_image(candidate, tile_size, upscaler, tileable=True)
        col, row = idx % ATLAS_COLS, idx // ATLAS_COLS
        atlas.paste(tile, (col * tile_size, row * tile_size))
        print(f"    [{idx:2d}] {stem:<14} ({tile.size[0]}x{tile.size[1]})")
    out_atlas.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(out_atlas, "PNG")
    side = ATLAS_COLS * tile_size
    print(f"  wrote {out_atlas} ({side}x{side})")


def compose_all_walls_atlas(source_dir: Path, tile_size: int, upscaler: str,
                            out_atlas: Path, cols: int = 16) -> None:
    """Compose every ``wall_NNN.png`` into a single ``cols``-wide atlas.

    Walls are upscaled with tileable=True so adjacent copies of any single
    tile still join seamlessly after the upscale.
    """
    walls = sorted(source_dir.glob("wall_*.png"))
    rows = (len(walls) + cols - 1) // cols
    atlas = Image.new("RGBA", (cols * tile_size, rows * tile_size), (0, 0, 0, 0))
    print(f"  full atlas: {len(walls)} walls in a {cols}x{rows} grid @ {tile_size}px tiles (tileable)")
    for idx, src in enumerate(walls):
        tile = upscale_image(src, tile_size, upscaler, tileable=True)
        col, row = idx % cols, idx // cols
        atlas.paste(tile, (col * tile_size, row * tile_size))
    out_atlas.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(out_atlas, "PNG")
    print(f"  wrote {out_atlas} ({atlas.size[0]}x{atlas.size[1]})")


def write_preview_html(source_dir: Path, out_html: Path, glob: str = "*.png") -> None:
    """Render PNGs in ``source_dir`` matching ``glob`` as labelled thumbnails."""
    pngs = sorted(source_dir.glob(glob))
    out_html.parent.mkdir(parents=True, exist_ok=True)
    rows = []
    for p in pngs:
        rows.append(
            f'<div class="tile"><img src="{p.resolve().as_uri()}" alt="{p.stem}">'
            f'<span>{p.stem}</span></div>'
        )
    html = (
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<title>Wolf3D dump preview</title>"
        "<style>"
        "body{font-family:sans-serif;background:#1c1c1c;color:#ddd;margin:0;padding:16px}"
        ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(160px,1fr));gap:12px}"
        ".tile{background:#252525;border:1px solid #333;padding:8px;text-align:center}"
        ".tile img{width:128px;height:128px;image-rendering:pixelated;display:block;margin:0 auto 4px}"
        ".tile span{font-family:monospace;font-size:13px;color:#9cdcfe}"
        "h1{font-size:18px;margin:0 0 12px;color:#fff}"
        "</style></head><body>"
        f"<h1>{len(pngs)} entries in {source_dir} (glob={glob})</h1>"
        f'<div class="grid">{"".join(rows)}</div>'
        "</body></html>"
    )
    out_html.write_text(html, encoding="utf-8")
    print(f"  wrote preview to {out_html}")


def export_sprites(source_dir: Path, tile_size: int, upscaler: str,
                   out_dir: Path, sprite_glob: str = "*.png") -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    pngs = sorted(p for p in source_dir.glob(sprite_glob))
    print(f"  sprites (glob={sprite_glob!r}):")
    for src in pngs:
        tile = upscale_image(src, tile_size, upscaler)
        dst = out_dir / src.name
        tile.save(dst, "PNG")
        print(f"    [ok] {src.name:<20} -> {dst} ({tile.size[0]}x{tile.size[1]})")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE,
                        help="Directory containing the raw 64x64 (or larger) Wolf3D PNGs.")
    parser.add_argument("--tile-size", type=int, default=DEFAULT_TILE_SIZE,
                        help="Target output size for each tile / sprite (default 128).")
    parser.add_argument("--upscaler", choices=("auto", "realesrgan", "lanczos", "nearest"),
                        default="auto",
                        help="How to grow images larger than their source.")
    parser.add_argument("--out-atlas", type=Path, default=DEFAULT_OUT_ATLAS,
                        help="Output PNG for the 4x4 wall atlas.")
    parser.add_argument("--out-sprites", type=Path, default=DEFAULT_OUT_SPRITES,
                        help="Output directory for upscaled sprite PNGs.")
    parser.add_argument("--no-atlas", action="store_true",
                        help="Skip atlas composition.")
    parser.add_argument("--no-sprites", action="store_true",
                        help="Skip per-sprite upscaling.")
    parser.add_argument("--all-walls-atlas", type=Path, default=None,
                        help="Output PNG for a single big atlas of every wall_NNN.png.")
    parser.add_argument("--all-walls-cols", type=int, default=16,
                        help="Column count for the all-walls atlas (default 16).")
    parser.add_argument("--sprite-range", default=None,
                        help="Restrict sprites_NNN.png export. Format: 'A-B' or 'A,B,C'.")
    parser.add_argument("--vswap", type=Path, default=None,
                        help="Path to VSWAP.WL6 (or .WL1 shareware).")
    parser.add_argument("--vswap-no-walls", action="store_true",
                        help="With --vswap, skip wall extraction (sprites only).")
    parser.add_argument("--vswap-no-sprites", action="store_true",
                        help="With --vswap, skip sprite extraction (walls only).")
    parser.add_argument("--list", action="store_true",
                        help="Show source PNGs + upscaler choice and exit.")
    parser.add_argument("--preview", type=Path, default=None,
                        help="Write an HTML grid of PNGs in --source.")
    parser.add_argument("--preview-glob", default="wall_*.png",
                        help="Glob filter for the preview (default: walls only).")
    args = parser.parse_args()

    if args.vswap is not None:
        print(f"[vswap] {args.vswap}")
        if not args.vswap.is_file():
            raise SystemExit(f"VSWAP file not found: {args.vswap}")
        extract_vswap(
            args.vswap, args.source,
            walls=not args.vswap_no_walls,
            sprites=not args.vswap_no_sprites,
        )

    if args.upscaler == "auto":
        upscaler = "realesrgan" if detect_realesrgan() else "lanczos"
    else:
        upscaler = args.upscaler
    print(f"[upscaler] using {upscaler}")

    print(f"[source] {args.source}")
    if not args.source.is_dir():
        raise SystemExit(f"Source directory does not exist: {args.source}")
    pngs = sorted(args.source.glob("*.png"))
    print(f"  {len(pngs)} PNGs found")
    if args.list:
        for p in pngs:
            print(f"    {p.name}")
        return

    if args.preview is not None:
        print(f"[preview] {args.preview} (glob={args.preview_glob!r})")
        write_preview_html(args.source, args.preview, args.preview_glob)
        return

    if not args.no_atlas:
        print(f"[atlas] {args.out_atlas}")
        compose_atlas(args.source, args.tile_size, upscaler, args.out_atlas)
    if args.all_walls_atlas is not None:
        print(f"[all-walls-atlas] {args.all_walls_atlas}")
        compose_all_walls_atlas(
            args.source, args.tile_size, upscaler,
            args.all_walls_atlas, cols=args.all_walls_cols,
        )
    if not args.no_sprites:
        if args.sprite_range:
            indices: list[int] = []
            for part in args.sprite_range.split(","):
                part = part.strip()
                if "-" in part:
                    a, b = part.split("-", 1)
                    indices.extend(range(int(a), int(b) + 1))
                elif part:
                    indices.append(int(part))
            wanted = {f"sprite_{i:03d}.png" for i in indices}
            print(f"[sprites] {args.out_sprites} (range={args.sprite_range})")
            args.out_sprites.mkdir(parents=True, exist_ok=True)
            for src in sorted(args.source.glob("sprite_*.png")):
                if src.name not in wanted:
                    continue
                tile = upscale_image(src, args.tile_size, upscaler)
                dst = args.out_sprites / src.name
                tile.save(dst, "PNG")
                print(f"    [ok] {src.name:<20} -> {dst} ({tile.size[0]}x{tile.size[1]})")
        else:
            print(f"[sprites] {args.out_sprites}")
            export_sprites(args.source, args.tile_size, upscaler, args.out_sprites)


if __name__ == "__main__":
    main()
