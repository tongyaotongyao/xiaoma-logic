"""Create small original feedback sounds without external assets or libraries."""

from pathlib import Path
import math
import struct
import wave


root = Path(__file__).resolve().parents[1]
out = root / "Content" / "ImportSource" / "Audio"
out.mkdir(parents=True, exist_ok=True)
rate = 44100


def write_sound(name, notes):
    duration = max(start + length for start, length, _ in notes) + 0.08
    samples = []
    for frame in range(int(duration * rate)):
        time = frame / rate
        value = 0.0
        for start, length, frequency in notes:
            elapsed = time - start
            if 0 <= elapsed < length:
                envelope = min(1.0, elapsed * 75) * (1 - elapsed / length) ** 2
                value += envelope * (
                    math.sin(2 * math.pi * frequency * elapsed)
                    + 0.22 * math.sin(4 * math.pi * frequency * elapsed)
                )
        samples.append(struct.pack("<h", round(max(-1, min(1, value * 0.22)) * 32767)))
    with wave.open(str(out / f"{name}.wav"), "wb") as audio:
        audio.setnchannels(1)
        audio.setsampwidth(2)
        audio.setframerate(rate)
        audio.writeframes(b"".join(samples))


write_sound("Place", [(0, 0.22, 587.33), (0.03, 0.19, 880)])
write_sound("Error", [(0, 0.23, 246.94), (0.12, 0.23, 196)])
write_sound("Win", [(0, 0.38, 523.25), (0.13, 0.36, 659.25),
                    (0.26, 0.48, 783.99), (0.40, 0.54, 1046.5)])
print(f"Wrote 3 feedback sounds to {out}")
