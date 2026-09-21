"""
generate_pbr_maps.py - thin CLI wrapper around RTX Remix's bundled local Image-to-Material
(I2M) model.

Runs the model through RTX Remix's own bundled Python interpreter (which already carries
torch/torchvision/PIL) as a subprocess, so this script itself has no third-party
dependencies and works with any system Python.

This install's model config only generates diffuse / normal_dx / roughness maps - it does
NOT generate AO or metallic, no matter what input it's given. See
AI/reference/rtx-remix-texture-gen.md for the full writeup, and pack_orm.py in this same
directory for combining the roughness output with AO/metallic (placeholder or supplied) into
a single UE-convention ORM texture.

Usage:
  generate_pbr_maps.py --input <file_or_dir> [--input <file_or_dir> ...] --output <dir>
                        [--remix-root <path>] [--max-size N] [--noise F] [--steps N] [--debug]

--input may be repeated and mixes files and directories freely (each is routed to the
underlying CLI's --file or --directory, which are mutually exclusive with each other, so
mixed input runs the tool twice under the hood - once per group).
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path

DEFAULT_REMIX_ROOT = Path(os.environ.get("RTX_REMIX_ROOT", r"C:\Program Files\NVIDIA Corporation\RTX Remix"))


def resolve_remix_paths(remix_root: Path) -> dict:
    exts = remix_root / "exts"
    pip_archive = exts / "omni.flux.pip_archive"
    ai_tools = exts / "lightspeed.trex.app.resources" / "deps" / "ai_tools" / "i2m" / "artifacts"

    paths = {
        "python_exe": remix_root / "kit" / "python" / "python.exe",
        "cli_py": pip_archive / "internal_pip_prebundle" / "remix" / "cli" / "cli.py",
        "internal_pip": pip_archive / "internal_pip_prebundle",
        "flux_pip": pip_archive / "flux_pip_prebundle",
        "model": ai_tools / "model.pt",
        "config": ai_tools / "config.yaml",
    }
    missing = [name for name, path in paths.items() if not path.exists()]
    if missing:
        raise FileNotFoundError(
            f"RTX Remix install at {remix_root} is missing: {', '.join(missing)}. "
            "Pass --remix-root or set RTX_REMIX_ROOT if it's installed elsewhere, or "
            "reinstall RTX Remix if a real path is missing these files."
        )
    return paths


def run_local_i2m(paths: dict, inputs: list[Path], flag: str, output: Path, args) -> int:
    cmd = [
        str(paths["python_exe"]), str(paths["cli_py"]), "local_i2m",
        "--model-artifact", str(paths["model"]),
        "--config-artifact", str(paths["config"]),
        "--output", str(output),
        "--max-size", str(args.max_size),
    ]
    for item in inputs:
        cmd += [flag, str(item)]
    if args.noise is not None:
        cmd += ["--noise", str(args.noise)]
    if args.steps is not None:
        cmd += ["--steps", str(args.steps)]
    if args.debug:
        cmd.append("--debug")

    env = dict(os.environ)
    env["PYTHONPATH"] = os.pathsep.join([str(paths["internal_pip"]), str(paths["flux_pip"])])

    print(f"Running: {' '.join(cmd)}")
    return subprocess.run(cmd, env=env, check=False).returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--input", "-i", action="append", required=True, type=Path,
                         help="Texture file or directory of textures. Repeatable.")
    parser.add_argument("--output", "-o", required=True, type=Path)
    parser.add_argument("--remix-root", type=Path, default=DEFAULT_REMIX_ROOT)
    parser.add_argument("--max-size", type=int, default=512,
                         help="Max square resolution for inference; larger tiles run patchwise (default: 512)")
    parser.add_argument("--noise", type=float, help="Noise level override (model default if omitted)")
    parser.add_argument("--steps", type=int, help="Denoising step count override (model default if omitted)")
    parser.add_argument("--debug", action="store_true", help="Fixed seed / deterministic algorithms")
    args = parser.parse_args()

    try:
        paths = resolve_remix_paths(args.remix_root)
    except FileNotFoundError as e:
        parser.error(str(e))
        return

    files = [p for p in args.input if p.is_file()]
    dirs = [p for p in args.input if p.is_dir()]
    missing = [p for p in args.input if not p.exists()]
    if missing:
        parser.error(f"Input path(s) not found: {', '.join(str(p) for p in missing)}")

    return_code = 0
    if files:
        return_code |= run_local_i2m(paths, files, "--file", args.output, args)
    if dirs:
        return_code |= run_local_i2m(paths, dirs, "--directory", args.output, args)

    sys.exit(return_code)


if __name__ == "__main__":
    main()
