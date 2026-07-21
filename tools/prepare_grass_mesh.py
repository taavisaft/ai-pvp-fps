"""Pack one dense Poly Haven tuft and three tiny accents into a runtime cluster.

The source glTF displays its variants in a row. This script ignores those display
translations, recentres each plant, and arranges four exact low-poly plants into
one small patch. Output format: magic, vertex/index counts, pos.xyz+uv.xy, uint32 indices.
"""

import json
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "_RESOURCES/grass/grass_medium_01_1k"
GLTF = SOURCE / "grass_medium_01_1k.gltf"
OUTPUT = ROOT / "models/grass/grass_medium_cluster.bin"
NAMES = (
    "grass_medium_01_small_a_LOD0",
    "grass_medium_01_tiny_c_LOD0",
    "grass_medium_01_tiny_e_LOD0",
    "grass_medium_01_tiny_f_LOD0",
)
OFFSETS = ((0.0, 0.0), (-0.10, 0.07), (0.09, 0.08), (0.03, -0.10))
ANGLES = (0.1, 2.0, 4.0, 5.5)
SCALES = (1.0, 1.12, 1.18, 1.10)


def read_accessor(doc, blob, index):
    acc = doc["accessors"][index]
    view = doc["bufferViews"][acc["bufferView"]]
    formats = {5123: "H", 5125: "I", 5126: "f"}
    widths = {"SCALAR": 1, "VEC2": 2, "VEC3": 3}
    code = formats[acc["componentType"]]
    width = widths[acc["type"]]
    size = struct.calcsize("<" + code) * width
    stride = view.get("byteStride", size)
    start = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    fmt = "<" + code * width
    return [struct.unpack_from(fmt, blob, start + i * stride)
            for i in range(acc["count"])]


def main():
    doc = json.loads(GLTF.read_text())
    blob = (SOURCE / doc["buffers"][0]["uri"]).read_bytes()
    nodes = {node["name"]: node for node in doc["nodes"]}
    vertices, indices = [], []

    for plant, (ox, oz), angle, scale in zip(NAMES, OFFSETS, ANGLES, SCALES):
        primitive = doc["meshes"][nodes[plant]["mesh"]]["primitives"][0]
        positions = read_accessor(doc, blob, primitive["attributes"]["POSITION"])
        uvs = read_accessor(doc, blob, primitive["attributes"]["TEXCOORD_0"])
        source_indices = read_accessor(doc, blob, primitive["indices"])
        cx = (min(p[0] for p in positions) + max(p[0] for p in positions)) * 0.5
        cz = (min(p[2] for p in positions) + max(p[2] for p in positions)) * 0.5
        base_y = min(p[1] for p in positions)
        c, s = math.cos(angle), math.sin(angle)
        base = len(vertices)
        for position, uv in zip(positions, uvs):
            x, y, z = ((position[0] - cx) * scale,
                       (position[1] - base_y) * scale,
                       (position[2] - cz) * scale)
            vertices.append((x * c - z * s + ox, y, x * s + z * c + oz,
                             uv[0], uv[1]))
        indices.extend(base + item[0] for item in source_indices)

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT.open("wb") as out:
        out.write(struct.pack("<4sII", b"GRT1", len(vertices), len(indices)))
        for vertex in vertices:
            out.write(struct.pack("<5f", *vertex))
        for index in indices:
            out.write(struct.pack("<I", index))
    print(f"{OUTPUT}: {len(vertices)} vertices, {len(indices) // 3} triangles")


if __name__ == "__main__":
    main()
