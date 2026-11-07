#!/usr/bin/env python3
"""Measures the impulse responses written by `RenderTest measure`, run from test/build."""
import numpy as np
from scipy import signal
from scipy.io import wavfile

NAMES = {0: "Hall", 1: "Plate", 3: "Chamber", 6: "60s Chamber", 7: "60s Mirror", 8: "90s Hall", 9: "90s Concert"}
BANDS = [125, 250, 500, 1000, 2000, 4000, 8000]

def load(m, t):
    fs, x = wavfile.read(f"ir_m{m}_t{t}.wav")
    return fs, x.astype(np.float64)

def rt60(x, fs, lo=None, hi=None):
    if lo:
        x = signal.sosfiltfilt(signal.butter(4, [lo, hi], "bandpass", fs=fs, output="sos"), x)
    e = np.cumsum((x ** 2)[::-1])[::-1]
    if e[0] <= 0: return float("nan")
    db = 10 * np.log10(e / e[0] + 1e-30)
    i0, i1 = np.argmax(db <= -5), np.argmax(db <= -25)
    if i1 <= i0: return float("nan")
    t = np.arange(i0, i1) / fs
    return -60 / np.polyfit(t, db[i0:i1], 1)[0]

def echo_density(x, fs, win=0.02):
    """Abel and Huang normalised echo density; 1 means Gaussian-dense."""
    n = int(win * fs); w = np.hanning(n); w /= w.sum(); out = []
    for i in range(0, len(x) - n, n // 2):
        s = x[i:i + n]; sd = np.sqrt(np.sum(w * s * s))
        out.append(np.sum(w * (np.abs(s) > sd)) / 0.3173 if sd > 0 else 0)
    return np.array(out), n / 2 / fs

def ringing(x, fs, tail_start, tail_len):
    """Spectrum peak over local median in the tail, dB. Lower is smoother; shows isolated modes."""
    a = int(tail_start * fs); seg = x[a:a + int(tail_len * fs)]
    f, p = signal.welch(seg, fs, nperseg=8192, noverlap=6144)
    k = (f > 150) & (f < 6000)
    db = 10 * np.log10(p[k] + 1e-30)
    med = signal.medfilt(db, 101)
    d = db - med
    return np.percentile(d, 99.5), d.max()

print(f"{'':12} " + " ".join(f"{b:>6}" for b in BANDS) + "   (RT60 s by octave; set Decay in brackets)")
for m, nm in NAMES.items():
    for t in (1, 2, 5):
        fs, x = load(m, t); mono = x.mean(axis=1)
        vals = [rt60(mono, fs, b / 1.414, min(b * 1.414, fs / 2 - 100)) for b in BANDS]
        print(f"{nm:12} " + " ".join(f"{v:6.2f}" for v in vals) + f"   [{t}s]  broad {rt60(mono, fs):.2f}")
print()
print(f"{'':12} {'dens>0.9 ms':>11} {'ring99.5':>9} {'ringMax':>8} {'corr':>6} {'L-R dB':>7} {'mono loss':>9} {'energy dB':>9} {'build ms':>8}")
for m, nm in NAMES.items():
    fs, x = load(m, 2); mono = x.mean(axis=1)
    d, hop = echo_density(mono, fs)
    idx = np.argmax(d > 0.9) if np.any(d > 0.9) else -1
    tail0 = 0.25; a = int(tail0 * fs); b = a + int(fs * 1.0)
    corr = np.corrcoef(x[a:b, 0], x[a:b, 1])[0, 1]
    bal = 10 * np.log10(np.sum(x[a:b, 0] ** 2) / np.sum(x[a:b, 1] ** 2))
    loss = 10 * np.log10(np.sum(mono ** 2) * 2 / np.sum(x ** 2))
    energy = 10 * np.log10(np.sum(x ** 2))
    r995, rmax = ringing(mono, fs, tail0, 1.2)
    env = signal.fftconvolve(mono ** 2, np.ones(int(.005 * fs)), "same")
    print(f"{nm:12} {idx * hop * 1000 if idx >= 0 else -1:11.0f} {r995:9.1f} {rmax:8.1f} {corr:6.2f} {bal:7.2f} {loss:9.2f} {energy:9.2f} {np.argmax(env) / fs * 1000:8.0f}")
