# RTX Remix texture generation (Normal / Roughness / packed ORM)

Generates a normal map and a roughness map from a single albedo/diffuse texture using the
Image-to-Material (I2M) AI model bundled with the local RTX Remix install, then packs the
result into a UE-convention ORM texture. Runs entirely offline via CLI — no RTX Remix UI
required (the UI's own AI-texture panel is prone to freezing/hanging; this is the same
model, invoked directly).

Tools: `AI/tools/generate_pbr_maps.py`, `AI/tools/pack_orm.py`.

## Prerequisites

- RTX Remix installed locally (default expected at
  `C:\Program Files\NVIDIA Corporation\RTX Remix`; override with `--remix-root` or the
  `RTX_REMIX_ROOT` env var if installed elsewhere).
- A CUDA-capable GPU is used automatically if present (falls back to CPU otherwise — much
  slower, but works).
- `generate_pbr_maps.py` has no third-party dependencies itself — it shells out to RTX
  Remix's own bundled Python (`kit/python/python.exe`), which already carries
  torch/torchvision/PIL. Run it with any Python (3.9+).
- `pack_orm.py` needs Pillow. Easiest: run it with the same bundled interpreter,
  `"<remix_root>\kit\python\python.exe" AI\tools\pack_orm.py ...`.

## What the model actually generates

The shipped model config
(`exts\lightspeed.trex.app.resources\deps\ai_tools\i2m\artifacts\config.yaml`) has:

```yaml
DATASET:
  OUT_MAPS: ['diffuse', 'normal_dx', 'roughness']
```

That's the ceiling of this model artifact — it **never produces AO or metallic**, regardless
of input. There is no metallic channel anywhere in the model's `ChannelType` enum, and AO
isn't in this config's `OUT_MAPS` even though the enum has an AO entry (presumably used by a
different config/model version, not this one). Don't spend time re-investigating this later;
if a future RTX Remix update ships a different `config.yaml` with more `OUT_MAPS`, that's the
one file to check first.

Practical implication: `pack_orm.py` fills AO and metallic with constants by default
(AO=1.0/no extra occlusion, metallic=0.0/dielectric) — correct for a non-metal like wood or
stone, wrong for anything that should look metallic or have baked cavity occlusion. Supply
`--ao`/`--metallic` as image paths for those cases; there's no way to get them from this
model.

The output normal map is `normal_dx` (DirectX / green-channel-down convention), which is what
Unreal expects — no channel flip needed.

## Usage

Generate normal + roughness for one texture (or a whole directory — repeat `--input`, mixes
files and directories freely):

```
python AI\tools\generate_pbr_maps.py ^
  --input "Assets\Textures\T_Wood_Tiling_01.png" ^
  --output "Assets\RTXRemix"
```

Pack into a single ORM texture (R=AO, G=Roughness, B=Metallic — matches the `ORMTexture`
parameter convention in `AI\build_wood_material.py`):

```
"C:\Program Files\NVIDIA Corporation\RTX Remix\kit\python\python.exe" AI\tools\pack_orm.py ^
  --dir "Assets\RTXRemix"
```

`--dir` batch mode finds every `*_roughness.png` in a folder and writes `<stem>_ORM.png` next
to it. For one-off control (real AO/metallic maps, non-default constants), use single-file
mode instead: `--roughness <path> --output <path> [--ao <path|0-1>] [--metallic <path|0-1>]`.

## Known quirks

- `--max-size` controls patch-splitting behavior for inputs larger than that resolution
  (default 512); it does **not** cap the output resolution — the model always upscales 4x
  regardless (`DATASET.DATA_SCALE` in `config.yaml`).
- The vendor CLI's own `--oversized-behavior` flag (in `remix/cli/cli.py`) is broken as
  shipped: it's declared with `type=OversizedBehavior`, an int-valued enum, but argparse
  hands it a string from the command line, so any value raises `ValueError` before inference
  even starts. `generate_pbr_maps.py` deliberately never passes this flag through and lets
  the CLI fall back to its coded default (patchwise). Don't add a `--oversized-behavior`
  passthrough without re-testing whether NVIDIA has fixed this upstream.
- Git Bash / MSYS: pass Windows-style paths (`C:/...`), not POSIX-translated ones (`/c/...`),
  to any subprocess that hands off to `python.exe` directly — the interpreter doesn't
  understand the `/c/` form.
- `--output` must not contain a space: `Game/hooks/pre-commit` rejects any staged path with a
  space or a character outside `A-Za-z0-9._/-`, so `Assets/RTXRemix` (not `Assets/RTX Remix`)
  is the committable convention.

## Not automated on purpose

There's no script here that auto-wires generated maps into a material asset. `pack_orm.py`'s
output still has to be pointed at deliberately (by a human or an agent reading the actual
texture set for a given material) — see `AI\build_wood_material.py` for the current
triplanar wood material, which as of this writing still uses hardcoded neutral placeholders
rather than these generated maps.
