"""
pack_orm.py - pack separate AO / Roughness / Metallic inputs into a single UE-convention
ORM texture (R=AO, G=Roughness, B=Metallic - matches the "ORMTexture" parameter in
AI/build_wood_material.py and Unreal's standard packed-mask layout).

Roughness must come from a real texture (e.g. generate_pbr_maps.py's output). AO and
Metallic each accept either an image path or a constant in [0, 1] - default AO=1.0 (no
extra occlusion) and Metallic=0.0 (dielectric), since the RTX Remix I2M model this pipeline
uses does not generate either channel (see AI/reference/rtx-remix-texture-gen.md).

Requires Pillow. Run with a Python that has it installed - the RTX Remix-bundled
interpreter works:
  "<remix_root>/kit/python/python.exe" pack_orm.py ...

Single file:
  pack_orm.py --roughness <path> --output <path> [--ao <path|0-1>] [--metallic <path|0-1>]

Batch (every *_roughness.png in a directory -> <stem>_ORM.png next to it):
  pack_orm.py --dir <path> [--ao <path|0-1>] [--metallic <path|0-1>] [--suffix _roughness.png]
"""
import argparse
from pathlib import Path

from PIL import Image


def _channel(value: str, size: tuple[int, int]) -> Image.Image:
    """Resolve an --ao/--metallic argument to a single-channel ("L") image of the given size."""
    try:
        level = float(value)
    except ValueError:
        img = Image.open(value).convert("L")
        return img if img.size == size else img.resize(size, Image.BICUBIC)

    if not 0.0 <= level <= 1.0:
        raise ValueError(f"Constant channel value must be in [0, 1], got {level}")
    return Image.new("L", size, round(level * 255))


def pack(roughness_path: Path, output_path: Path, ao: str, metallic: str) -> Path:
    roughness = Image.open(roughness_path).convert("L")
    orm = Image.merge("RGB", (_channel(ao, roughness.size), roughness, _channel(metallic, roughness.size)))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    orm.save(output_path)
    return output_path


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--roughness", type=Path, help="Roughness texture (single-file mode)")
    parser.add_argument("--output", type=Path, help="Output ORM path (single-file mode)")
    parser.add_argument("--dir", type=Path, help="Directory to batch-process (finds *<suffix> files)")
    parser.add_argument("--suffix", default="_roughness.png", help="Roughness filename suffix for --dir mode")
    parser.add_argument("--ao", default="1.0", help="AO image path or constant 0-1 (default: 1.0)")
    parser.add_argument("--metallic", default="0.0", help="Metallic image path or constant 0-1 (default: 0.0)")
    args = parser.parse_args()

    if args.dir:
        roughness_files = sorted(args.dir.glob(f"*{args.suffix}"))
        if not roughness_files:
            parser.error(f"No files matching *{args.suffix} found in {args.dir}")
        for roughness_path in roughness_files:
            stem = roughness_path.name[: -len(args.suffix)]
            output_path = pack(roughness_path, args.dir / f"{stem}_ORM.png", args.ao, args.metallic)
            print(f"Wrote {output_path}")
    else:
        if not args.roughness or not args.output:
            parser.error("--roughness and --output are required outside of --dir mode")
        print(f"Wrote {pack(args.roughness, args.output, args.ao, args.metallic)}")


if __name__ == "__main__":
    main()
