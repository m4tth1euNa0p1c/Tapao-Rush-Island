"""Generate the shot of the bean's pirate pump shotgun (T-167): SW_Shotgun_Fire, loud and distinct from the musket.

Made by script for the project (numpy synthesis, no external source, no licence to credit): 44.1 kHz, mono, 16 bit
PCM WAV, peak normalised to -1 dBFS (a gun shot: the loudest effect of the game, the mix lowers it through SC_Effects).

Layers, from the front of the shot to its tail:
  crack   white noise, 6 ms, bright (high-passed): the blast leaving the muzzle
  boom    noise low-passed with a falling cut-off (2.5 kHz -> 300 Hz), decay 0.14 s: the body of the shot
  thump   sine 62 -> 36 Hz, decay 0.22 s: the chest punch a musket shot does not have
  tail    dark noise, decay 0.7 s, and one slap-back echo at 0.21 s (-11 dB): open air on a beach

Different seeds give variants: SW_Shotgun_Fire (seed 167), SW_Shotgun_Fire_B (seed 168).
Usage: python scripts/props/shotgun_shot_sound.py [output dir]   (default: art/audio/weapons)
Then import with scripts/props/import_shotgun_sounds.py.
"""

import sys
import wave
from pathlib import Path

import numpy as np

RATE = 44100
PEAK_DBFS = -1.0
LENGTH_S = 1.7


def one_pole_lowpass(signal, cutoffs):
	"""Low-pass whose cut-off (Hz) may change per sample."""
	out = np.empty_like(signal)
	cutoffs = np.broadcast_to(cutoffs, signal.shape)
	alpha = 1.0 - np.exp(-2.0 * np.pi * cutoffs / RATE)
	state = 0.0
	for index, sample in enumerate(signal):
		state += alpha[index] * (sample - state)
		out[index] = state
	return out


def highpass(signal, cutoff):
	return signal - one_pole_lowpass(signal, cutoff)


def shot(seed):
	rng = np.random.default_rng(seed)
	n = int(LENGTH_S * RATE)
	t = np.arange(n) / RATE

	crack = highpass(rng.standard_normal(n), 1800.0) * np.exp(-t / 0.006)

	sweep = 300.0 + 2200.0 * np.exp(-t / 0.05)
	boom = one_pole_lowpass(rng.standard_normal(n), sweep)
	boom = one_pole_lowpass(boom, sweep) * np.exp(-t / 0.14) * 3.2

	freq = 36.0 + 26.0 * np.exp(-t / 0.06)
	thump = np.sin(2.0 * np.pi * np.cumsum(freq) / RATE) * np.exp(-t / 0.22) * 0.9

	tail = one_pole_lowpass(rng.standard_normal(n), 700.0) * np.exp(-t / 0.7) * 0.9

	dry = crack * 0.8 + boom + thump + tail
	# soft attack over 1 ms (no click), slap-back echo off a reef or a hut
	dry[: int(0.001 * RATE)] *= np.linspace(0.0, 1.0, int(0.001 * RATE))
	delay = int(0.21 * RATE)
	echo = np.zeros(n)
	echo[delay:] = one_pole_lowpass(dry[:-delay], 1200.0) * 10 ** (-11.0 / 20.0)
	mixed = dry + echo
	# gentle saturation: a dense, loud body without a harsh clip
	mixed = np.tanh(mixed * 1.6)
	fade = int(0.08 * RATE)
	mixed[-fade:] *= 0.5 + 0.5 * np.cos(np.linspace(0.0, np.pi, fade))
	return mixed / np.max(np.abs(mixed)) * 10 ** (PEAK_DBFS / 20.0)


def write(path, signal):
	path.parent.mkdir(parents=True, exist_ok=True)
	pcm = np.clip(signal * 32767.0, -32768, 32767).astype("<i2")
	with wave.open(str(path), "wb") as out:
		out.setnchannels(1)
		out.setsampwidth(2)
		out.setframerate(RATE)
		out.writeframes(pcm.tobytes())
	print(f"{path}  {len(signal) / RATE:.2f} s")


def main():
	folder = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / "art" / "audio" / "weapons"
	write(folder / "SW_Shotgun_Fire.wav", shot(167))
	write(folder / "SW_Shotgun_Fire_B.wav", shot(168))


if __name__ == "__main__":
	main()
