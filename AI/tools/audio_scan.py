"""Inspect source recordings before curating them into Assets/Audio.

Foley libraries usually ship several takes in one file ("Sponge 05.wav" is a
dozen slaps). This finds the individual hits by their energy envelope and
prints, per hit: start/end, peak level, loudness (RMS), and spectral centroid
(a rough "how bright" number: a wet slap sits ~1-3 kHz, a thud under 500 Hz).
The slice times it prints are exactly what AI/audio_manifest.py's `slice=`
field takes.

    python AI/tools/audio_scan.py <file-or-glob> [...] [--min-gap 0.12] [--thresh -30]

Needs numpy, scipy and soundfile (pip install soundfile).
"""

import argparse
import glob
import os
import sys

import numpy as np
import soundfile as sf


def load_mono(path):
    data, rate = sf.read(path, always_2d=True, dtype="float32")
    return data.mean(axis=1), rate


def envelope_db(x, rate, win_s=0.005):
    win = max(1, int(rate * win_s))
    padded = np.pad(x.astype(np.float64) ** 2, (win // 2, win - win // 2 - 1), mode="edge")
    kernel = np.ones(win) / win
    rms = np.sqrt(np.convolve(padded, kernel, mode="valid"))
    return 20.0 * np.log10(np.maximum(rms, 1e-9))


def find_hits(x, rate, thresh_db, min_gap_s, min_len_s=0.03):
    """Segments where the envelope is within thresh_db of the file's peak envelope."""
    env = envelope_db(x, rate)
    gate = env > (env.max() + thresh_db)
    hits = []
    i, n = 0, len(gate)
    min_gap = int(min_gap_s * rate)
    while i < n:
        if not gate[i]:
            i += 1
            continue
        start = i
        quiet = 0
        while i < n and quiet < min_gap:
            quiet = 0 if gate[i] else quiet + 1
            i += 1
        end = i - quiet
        if (end - start) / rate >= min_len_s:
            hits.append((start, end))
    return hits


def centroid_hz(x, rate):
    if len(x) < 64:
        return 0.0
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    freqs = np.fft.rfftfreq(len(x), 1.0 / rate)
    # Audible band only: 96/192 kHz field recordings carry ultrasonic noise that
    # would otherwise drag every centroid up.
    band = (freqs >= 20.0) & (freqs <= 16000.0)
    spec, freqs = spec[band], freqs[band]
    return float((spec * freqs).sum() / max(spec.sum(), 1e-12))


def describe(path, thresh_db, min_gap_s):
    x, rate = load_mono(path)
    info = sf.info(path)
    print(f"\n{path}\n  {info.samplerate} Hz, {info.channels} ch, {info.subtype}, {info.duration:.2f}s,"
          f" peak {20*np.log10(max(np.abs(x).max(),1e-9)):.1f} dBFS")
    for k, (s, e) in enumerate(find_hits(x, rate, thresh_db, min_gap_s)):
        seg = x[s:e]
        peak = 20 * np.log10(max(np.abs(seg).max(), 1e-9))
        rms = 20 * np.log10(max(np.sqrt(np.mean(seg ** 2)), 1e-9))
        print(f"  [{k:2d}] {s/rate:7.3f}-{e/rate:7.3f}s  len {1000*(e-s)/rate:6.0f}ms"
              f"  peak {peak:6.1f}  rms {rms:6.1f}  centroid {centroid_hz(seg, rate):6.0f} Hz")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="+")
    ap.add_argument("--thresh", type=float, default=-30.0, help="gate, dB below the file's loudest moment")
    ap.add_argument("--min-gap", type=float, default=0.12, help="silence (s) that separates two hits")
    args = ap.parse_args()
    for pattern in args.paths:
        for path in sorted(glob.glob(pattern)) or [pattern]:
            if os.path.isfile(path):
                try:
                    describe(path, args.thresh, args.min_gap)
                except Exception as exc:  # a bad file shouldn't stop a batch scan
                    print(f"\n{path}\n  ERROR {exc}", file=sys.stderr)


if __name__ == "__main__":
    main()
