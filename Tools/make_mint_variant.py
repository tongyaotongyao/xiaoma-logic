"""Make a distinct sixth glTF material by tinting the optimized blue mesh."""

import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1] / "Content" / "ImportSource" / "Characters"
SOURCE = ROOT / "Blue.glb"
TARGET = ROOT / "Mint.glb"


def main() -> None:
    data = SOURCE.read_bytes()
    magic, version, total = struct.unpack_from("<4sII", data)
    if magic != b"glTF" or version != 2 or total != len(data):
        raise ValueError("Invalid source GLB")
    json_length, json_type = struct.unpack_from("<II", data, 12)
    if json_type != 0x4E4F534A:
        raise ValueError("Missing JSON chunk")
    document = json.loads(data[20 : 20 + json_length])
    document["materials"][0]["name"] = "Character_Mint"
    pbr = document["materials"][0].setdefault("pbrMetallicRoughness", {})
    pbr["baseColorFactor"] = [0.45, 1.0, 0.68, 1.0]
    document.setdefault("asset", {})["generator"] = "XiaomaLogic mint material variant"

    encoded = json.dumps(document, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    encoded += b" " * ((-len(encoded)) % 4)
    remaining = data[20 + json_length :]
    result = (
        struct.pack("<4sII", b"glTF", 2, 20 + len(encoded) + len(remaining))
        + struct.pack("<II", len(encoded), 0x4E4F534A)
        + encoded
        + remaining
    )
    TARGET.write_bytes(result)
    print(f"Wrote {TARGET} ({len(result)} bytes)")


if __name__ == "__main__":
    main()
