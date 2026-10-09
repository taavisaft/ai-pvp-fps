#!/usr/bin/env python3
"""Rebuild the CC0 footstep WAV banks; requires ffmpeg on PATH. No network access."""
from array import array
from pathlib import Path
import math
import struct
import subprocess
import sys
import wave

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'sounds' / 'footsteps'
SR = 44100

def convert(source, output, rate=1.0, cutoff=8000, limit=.36):
    result = subprocess.run(['ffmpeg', '-v', 'error', '-i', str(source), '-ac', '1',
        '-ar', str(SR), '-af', f'highpass=f=45,lowpass=f={cutoff}',
        '-f', 'f32le', '-'], check=True, capture_output=True)
    samples = array('f')
    samples.frombytes(result.stdout)
    if sys.byteorder != 'little':
        samples.byteswap()
    # Keep the main contact, remove leading silence, retain a short release tail.
    peak = max(abs(v) for v in samples)
    first = next(i for i,v in enumerate(samples) if abs(v) > peak*.025)
    first = max(0,first-int(.003*SR))
    samples = samples[first:]
    count = min(int(len(samples)/rate),int(limit*SR))
    resampled = []
    for i in range(count):
        p = i*rate
        a = int(p)
        b = min(a+1,len(samples)-1)
        resampled.append(samples[a]+(samples[b]-samples[a])*(p-a))
    # Small fade protects trimmed contacts; matching energy avoids loud odd takes.
    for i in range(count):
        resampled[i] *= min(1,i/(SR*.002),(count-1-i)/(SR*.018))
    rms = math.sqrt(sum(v*v for v in resampled)/count)
    gain = min(.065/rms,.50/max(abs(v) for v in resampled))
    pcm = [round(v*gain*32767) for v in resampled]
    with wave.open(str(output),'wb') as f:
        f.setparams((1,2,SR,0,'NONE','not compressed'))
        f.writeframes(struct.pack(f'<{count}h',*pcm))
    print(f'{output.name}: {count/SR:.3f}s, peak {max(abs(v) for v in pcm)/32768:.3f}')

for i in range(5):
    convert(DEST/'source'/f'footstep_grass_{i:03d}.ogg',DEST/f'grass_{i+1:02d}.wav')
# Eight processed variations of ONE gravel recording, not eight recorded takes.
for i in range(8):
    convert(DEST/'source'/'gravel.ogg',DEST/f'gravel_{i+1:02d}.wav',
            rate=.94+i*.018, cutoff=5800+i*300, limit=.38)
