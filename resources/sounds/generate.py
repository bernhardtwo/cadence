"""Generates the two alarm sounds bundled with Cadence.

Run from anywhere: python resources/sounds/generate.py
Both files are pure synthesized tones, 44.1 kHz, 16 bit, mono.
"""

import math
import struct
import wave
from pathlib import Path

RATE = 44100


def tone(frequency, seconds, amplitude, attack=0.008, release=0.12):
    samples = []
    count = int(RATE * seconds)
    for i in range(count):
        t = i / RATE
        env = 1.0
        if t < attack:
            env = t / attack
        remaining = seconds - t
        if remaining < release:
            env *= remaining / release
        decay = math.exp(-3.0 * t / seconds)
        value = math.sin(2 * math.pi * frequency * t)
        value += 0.25 * math.sin(2 * math.pi * frequency * 2 * t)
        samples.append(amplitude * env * decay * value / 1.25)
    return samples


def silence(seconds):
    return [0.0] * int(RATE * seconds)


def write(path, samples):
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        frames = bytearray()
        for value in samples:
            clipped = max(-1.0, min(1.0, value))
            frames += struct.pack("<h", int(clipped * 32767))
        out.writeframes(bytes(frames))


def main():
    here = Path(__file__).resolve().parent
    chime = tone(880.0, 0.16, 0.55) + silence(0.04) + tone(1318.5, 0.32, 0.55)
    write(here / "chime.wav", chime)
    soft = tone(659.3, 0.42, 0.32, attack=0.03, release=0.2)
    write(here / "soft.wav", soft)


if __name__ == "__main__":
    main()
