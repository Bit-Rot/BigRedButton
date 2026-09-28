"""Turn source recordings into game-ready WAVs, per AI/audio_manifest.py.

    python AI/tools/prep_audio.py            # everything
    python AI/tools/prep_audio.py Octo_Arm   # only sounds whose name contains this

For each manifest entry:
  1. read the source (wav/ogg/flac/aif), cut the slice
  2. downmix to mono (everything spatialised is mono: a stereo file panned in
     3D just collapses anyway, and costs double)
  3. varispeed pitch shift, if asked (resample, like slowing tape)
  4. resample to 48 kHz
  5. high-pass (DC and rumble the speakers can't use but the limiter can see)
  6. trim leading silence — latency on an impact is the one thing players feel
  7. one-shots: cut the tail where it decays into the noise floor, fade out;
     loops: fold an equal-power crossfade into the seam so it loops cleanly
  8. normalise to the preset's loudness target, under its peak ceiling
  9. write 16-bit PCM

and writes Assets/Audio/CREDITS.md listing every file's origin and licence.

Deterministic: same manifest + same sources = byte-identical output.
Needs numpy, scipy and soundfile.
"""

import os
import sys

import numpy as np
import soundfile as sf
from scipy import signal

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "AI"))
import audio_manifest as M  # noqa: E402

RATE = 48000

# Longest source-path prefix wins. Licence notes are for CREDITS.md — they
# summarise, the pack's own licence file is authoritative.
LICENCES = [
    ("Miscellaneous & Uncategorized/GameAudioGDC/", "Sonniss GDC Game Audio Bundle",
     "Royalty-free, commercial use, no attribution required (Sonniss GDC bundle licence)."),
    ("Miscellaneous & Uncategorized/PMSFX Sampler/", "PMSFX Sampler (June 2020)",
     "Royalty-free sampler licence from PMSFX."),
    ("SFX/GameDevMarket/", "GameDev Market",
     "GameDev Market standard licence: use in games permitted; do not redistribute the raw files."),
    ("SFX/EpicStockMedia/", "Epic Stock Media",
     "Epic Stock Media EULA: use in games permitted; do not redistribute the raw files."),
    ("SFX/Misc/ZombieHorrorPackageLight/", "Zombie Horror Package Light",
     "Purchased pack licence: use in games permitted; do not redistribute the raw files."),
    ("SFX/Misc/", "Purchased SFX pack (see source folder)",
     "Purchased pack licence: use in games permitted; do not redistribute the raw files."),
    ("Ambience/Ambiences_Sounds/", "Ambiences Sounds pack",
     "Purchased pack licence: use in games permitted; do not redistribute the raw files."),
    ("SFX/Kenney/", "Kenney", "CC0 1.0 (public domain)."),
]


def licence_for(src):
    for prefix, pack, note in LICENCES:
        if src.startswith(prefix):
            return pack, note
    return "Unknown", "CHECK THE SOURCE PACK'S LICENCE."


def db(x):
    return 20.0 * np.log10(max(float(x), 1e-12))


def read_source(spec):
    path = os.path.join(M.SOURCE_ROOT, spec["src"])
    data, rate = sf.read(path, always_2d=True, dtype="float64")
    if spec["slice"]:
        start, end = spec["slice"]
        data = data[int(start * rate):int(end * rate)]
    if len(data) == 0:
        raise ValueError("slice is empty")
    return data, rate


def resample(x, from_rate, to_rate):
    if from_rate == to_rate:
        return x
    g = np.gcd(int(from_rate), int(to_rate))
    return signal.resample_poly(x, to_rate // g, from_rate // g, axis=0)


def pitch_shift(x, semitones):
    """Varispeed: play faster/slower, so pitch and length move together."""
    if not semitones:
        return x
    ratio = 2.0 ** (semitones / 12.0)
    # Resample to 1/ratio of the length at the same nominal rate.
    up, down = 1000, int(round(1000 * ratio))
    return signal.resample_poly(x, up, down, axis=0)


def highpass(x, hz):
    if not hz:
        return x
    sos = signal.butter(2, hz, btype="highpass", fs=RATE, output="sos")
    return signal.sosfiltfilt(sos, x, axis=0)


def envelope(x, win_s=0.005):
    mono = np.abs(x).max(axis=1)
    win = max(1, int(RATE * win_s))
    return np.convolve(mono, np.ones(win) / win, mode="same")


def trim_head(x, rel_db=-45.0, preroll_s=0.002):
    env = envelope(x)
    above = np.nonzero(env > env.max() * 10 ** (rel_db / 20.0))[0]
    start = max(0, above[0] - int(preroll_s * RATE)) if len(above) else 0
    return x[start:]


def trim_tail(x, rel_db=-55.0):
    env = envelope(x, 0.02)
    above = np.nonzero(env > env.max() * 10 ** (rel_db / 20.0))[0]
    end = above[-1] + int(0.02 * RATE) if len(above) else len(x)
    return x[:min(len(x), end)]


def fades(x, fade_in_ms, fade_out_ms):
    n_in = min(len(x), int(RATE * fade_in_ms / 1000.0))
    n_out = min(len(x), int(RATE * fade_out_ms / 1000.0))
    x = x.copy()
    if n_in:
        x[:n_in] *= np.linspace(0.0, 1.0, n_in)[:, None]
    if n_out:
        # Cosine: the tail dies away rather than stopping on a ramp's corner.
        x[-n_out:] *= (0.5 * (1.0 + np.cos(np.linspace(0.0, np.pi, n_out))))[:, None]
    return x


def make_loop(x, xfade_s):
    """Fold the last xfade_s into the first with an equal-power crossfade. The
    result starts where the original's tail was fading in, so the end of the
    file runs straight back into its own start with no seam."""
    n = int(xfade_s * RATE)
    if len(x) < 3 * n:
        n = len(x) // 3
    body, tail = x[:-n], x[-n:]
    t = np.linspace(0.0, np.pi / 2.0, n)[:, None]
    head = body[:n] * np.sin(t) + tail * np.cos(t)
    return np.concatenate([head, body[n:]])


def normalise(x, preset, is_loop):
    if is_loop:
        loudness = np.sqrt(np.mean(x ** 2))
    else:
        # "How loud is the hit": the loudest 50 ms, not the whole file — a long
        # quiet tail would otherwise make a short sharp hit come out too hot.
        win = int(0.05 * RATE)
        power = np.convolve((x ** 2).mean(axis=1), np.ones(win) / win, mode="valid") if len(x) > win else [np.mean(x ** 2)]
        loudness = np.sqrt(np.max(power))
    gain_db = preset["target_db"] - db(loudness)
    peak_db = db(np.abs(x).max())
    gain_db = min(gain_db, preset["ceiling_db"] - peak_db)
    return x * 10 ** (gain_db / 20.0), gain_db


def process(spec):
    preset = M.PRESETS[spec["preset"]]
    is_loop = "loop_xfade" in preset

    x, rate = read_source(spec)
    if preset.get("mono", True):
        x = x.mean(axis=1, keepdims=True)
    x = pitch_shift(x, spec["pitch"])
    x = resample(x, rate, RATE)
    x = highpass(x, preset.get("highpass"))

    if is_loop:
        x = make_loop(x, preset["loop_xfade"])
    else:
        x = trim_head(x)
        x = trim_tail(x)
        max_len = preset.get("max_len")
        if max_len and len(x) > max_len * RATE:
            x = x[:int(max_len * RATE)]
        x = fades(x, *preset["fade_ms"])

    x, gain_db = normalise(x, preset, is_loop)
    return np.clip(x, -1.0, 1.0), gain_db


def main():
    only = sys.argv[1] if len(sys.argv) > 1 else None
    credits = []
    failures = 0

    for spec in M.SOUNDS:
        out_dir = os.path.join(REPO, M.GROUPS[spec["group"]][0])
        out_path = os.path.join(out_dir, spec["name"] + ".wav")
        pack, note = licence_for(spec["src"])
        credits.append((spec, pack, note))
        if only and only not in spec["name"]:
            continue
        try:
            x, gain = process(spec)
        except Exception as exc:
            failures += 1
            print(f"FAIL {spec['name']}: {exc}")
            continue
        os.makedirs(out_dir, exist_ok=True)
        sf.write(out_path, x, RATE, subtype="PCM_16")
        print(f"ok   {spec['name']:34} {len(x)/RATE:5.2f}s  gain {gain:+5.1f} dB  <- {os.path.basename(spec['src'])}")

    write_credits(credits)
    print(f"\n{len(M.SOUNDS) - failures} ok, {failures} failed")
    return 1 if failures else 0


def write_credits(credits):
    lines = [
        "# Audio credits",
        "",
        "Generated by `AI/tools/prep_audio.py` from `AI/audio_manifest.py` — do not edit by hand.",
        "",
        "Every file under `Assets/Audio/SFX` is cut, cleaned and level-matched from the source",
        "recording listed here. Paths are relative to the Resources audio library",
        f"(`{M.SOURCE_ROOT}`). The licence column summarises; each pack's own licence file",
        "is authoritative. Packs marked \"do not redistribute the raw files\" are why this",
        "repository must stay private.",
        "",
        "| File | Source | Slice (s) | Pack | Licence |",
        "|---|---|---|---|---|",
    ]
    for spec, pack, note in credits:
        cut = "%.3f–%.3f" % spec["slice"] if spec["slice"] else "whole"
        pitch = f", {spec['pitch']:+g} st" if spec["pitch"] else ""
        lines.append(f"| `{spec['name']}.wav` | `{spec['src']}` | {cut}{pitch} | {pack} | {note} |")
    path = os.path.join(REPO, "Assets", "Audio", "CREDITS.md")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    sys.exit(main())
