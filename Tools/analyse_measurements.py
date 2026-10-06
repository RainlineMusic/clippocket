#!/usr/bin/env python3
"""Read ClipMeasurements interleaved input/output doubles; print a spur table.
Requires numpy and scipy. This metric includes folded harmonics, modulation and
numerical/leakage residuals. It does not measure perceived quality or total THD.
"""
import argparse
import csv
import sys
from pathlib import Path
import numpy as np
from scipy.signal.windows import blackmanharris

parser = argparse.ArgumentParser()
parser.add_argument('folder', type=Path)
parser.add_argument('--csv', type=Path)
args = parser.parse_args()
rows = []

def analyse(y, frequency):
    bins = np.fft.rfftfreq(len(y), 1 / 48000)
    magnitudes = np.abs(np.fft.rfft(y * blackmanharris(len(y))))
    fundamental = magnitudes[np.abs(bins - frequency) < 20].max()
    allowed = bins < 20
    for harmonic in range(1, 300, 2):
        if harmonic * frequency >= 24000:
            break
        allowed |= np.abs(bins - harmonic * frequency) < 20
    index = np.argmax(np.where(allowed, 0, magnitudes))
    return float(20 * np.log10(max(1e-30, magnitudes[index] / fundamental))), float(bins[index]), float(np.max(np.abs(y)))

for quality in range(6):
    for model in range(2):
        for test, frequency in enumerate((997, 7000, 15000)):
            path = args.folder / f'q{quality}-m{model}-t{test}.f64'
            if not path.exists():
                continue
            data = np.fromfile(path, dtype=np.float64).reshape(-1, 2)
            spur, hz, peak = analyse(data[:, 1], frequency)
            rows.append([str(2 << quality) + 'x', 'Clean' if model == 0 else 'Perceptual', frequency, round(spur, 1), round(hz, 1), round(peak, 6)])
            if quality == 0 and model == 0:
                spur, hz, peak = analyse(np.clip(data[:, 0], -1, 1), frequency)
                rows.append(['1x', 'Naive hard clamp', frequency, round(spur, 1), round(hz, 1), round(peak, 6)])

columns = ['quality', 'model', 'tone_hz', 'largest_nonharmonic_spur_dbc', 'spur_hz', 'sample_peak']
writer = csv.writer(sys.stdout)
writer.writerow(columns)
writer.writerows(rows)
if args.csv:
    with args.csv.open('w', newline='') as file:
        writer = csv.writer(file)
        writer.writerow(columns)
        writer.writerows(rows)
