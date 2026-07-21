#!/usr/bin/env python3
"""Build game-sized Maple PBR textures without touching the source asset."""

from pathlib import Path
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "_RESOURCES/trees/Maple/Map"
OUT = ROOT / "models/trees/maple/textures"
OUT.mkdir(parents=True, exist_ok=True)


def fit(name: str, output: str, size: tuple[int, int], quality: int = 90) -> None:
    image = Image.open(SRC / name).convert("RGB")
    image = image.resize(size, Image.Resampling.LANCZOS)
    image.save(OUT / output, quality=quality, optimize=True)


# The leaf color JPEG has a matching opacity sheet. Store alpha in base color so
# glTF and the custom renderer need one texture fetch for the cutout.
leaf = Image.open(SRC / "Acer_X_freemanii_Leaf_Green.jpeg").convert("RGB")
opacity = Image.open(SRC / "Acer_X_freemanii_Leaf_Green_Opacity.jpeg").convert("L")
leaf = leaf.resize((2048, 2048), Image.Resampling.LANCZOS)
opacity = opacity.resize((2048, 2048), Image.Resampling.LANCZOS)
leaf.putalpha(opacity)
leaf.save(OUT / "leaf_rgba.png", optimize=True)

fit("Acer_X_freemanii_Leaf_Green_Normal.jpeg", "leaf_normal.jpg", (2048, 2048))
fit("Acer_X_freemanii_Trunk.jpeg", "trunk.jpg", (1024, 2048))
fit("Acer_X_freemanii_Trunk_Normal.jpeg", "trunk_normal.jpg", (1024, 2048))

# Source maps are gloss (white = smooth); glTF consumes roughness (white = rough).
for source, output, size in (
    ("Acer_X_freemanii_Leaf_Green_Gloss.jpeg", "leaf_roughness.jpg", (2048, 2048)),
    ("Acer_X_freemanii_Trunk_Gloss.jpeg", "trunk_roughness.jpg", (1024, 2048)),
):
    gloss = Image.open(SRC / source).convert("L").resize(size, Image.Resampling.LANCZOS)
    ImageOps.invert(gloss).save(OUT / output, quality=90, optimize=True)

print(f"Maple textures prepared in {OUT}")
