"""Shared signal helpers of the T-259 audio scripts (numpy, scipy, soundfile; no Unreal, no Blender).

Loading at 48 kHz, envelopes, fades, filters, BS.1770 loudness, seamless loops, simple convolution reverb, and the
measures written in the manifests. Everything is deterministic: random draws come from generators the callers seed.
"""
from __future__ import annotations

import hashlib
import math
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import butter, fftconvolve, resample_poly, sosfilt, sosfiltfilt

RATE = 48000


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load(path, rate=RATE, mono=True):
    """Float64 samples (frames, channels) at `rate`, DC removed; mono = channel mean"""
    audio, source_rate = sf.read(str(path), dtype="float64", always_2d=True)
    if mono:
        audio = audio.mean(axis=1, keepdims=True)
    if source_rate != rate:
        div = math.gcd(int(source_rate), rate)
        audio = resample_poly(audio, rate // div, int(source_rate) // div, axis=0)
    if not len(audio) or not np.isfinite(audio).all():
        raise ValueError(f"Invalid audio: {path}")
    return audio - audio.mean(axis=0)


def mono(audio):
    return audio[:, 0] if audio.ndim == 2 else audio


def seconds(count):
    return count / RATE


def samples(time_s):
    return int(round(time_s * RATE))


def envelope(signal, window_s=0.01):
    """RMS per window (no overlap) of a 1-D signal, and the window length in seconds"""
    n = max(1, samples(window_s))
    k = len(signal) // n
    if k == 0:
        return np.array([np.sqrt(np.mean(signal ** 2))]), window_s
    return np.sqrt(np.mean(signal[: k * n].reshape(k, n) ** 2, axis=1)), window_s


def db(value):
    return 20.0 * np.log10(np.maximum(np.abs(value), 1e-12))


def gain_db(value_db):
    return 10.0 ** (value_db / 20.0)


def fade(signal, fade_in_s=0.0, fade_out_s=0.0):
    """Raised cosine fades (1-D or 2-D)"""
    out = signal.copy()
    for length, head in ((samples(fade_in_s), True), (samples(fade_out_s), False)):
        length = min(length, len(out))
        if length <= 1:
            continue
        ramp = 0.5 - 0.5 * np.cos(np.linspace(0.0, np.pi, length))
        if out.ndim == 2:
            ramp = ramp[:, None]
        if head:
            out[:length] *= ramp
        else:
            out[-length:] *= ramp[::-1]
    return out


def butter_filter(signal, kind, cutoff, order=4, zero_phase=False):
    """kind: lowpass / highpass / bandpass (cutoff = (lo, hi))"""
    sos = butter(order, cutoff, btype=kind, fs=RATE, output="sos")
    return sosfiltfilt(sos, signal, axis=0) if zero_phase else sosfilt(sos, signal, axis=0)


def k_weight(signal):
    """ITU-R BS.1770 K-weighting at RATE (pre-filter shelf + RLB high-pass)"""
    f0, gain, q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    k = math.tan(math.pi * f0 / RATE)
    vh, vb = 10 ** (gain / 20), (10 ** (gain / 20)) ** 0.4996667741545416
    a0 = 1 + k / q + k * k
    shelf_b = [(vh + vb * k / q + k * k) / a0, 2 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0]
    shelf_a = [1, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0]
    f0, q = 38.13547087602444, 0.5003270373238773
    k = math.tan(math.pi * f0 / RATE)
    a0 = 1 + k / q + k * k
    from scipy.signal import lfilter
    out = lfilter(shelf_b, shelf_a, signal, axis=0)
    return lfilter([1, -2, 1], [1, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0], out, axis=0)


def loudness(signal):
    """Integrated loudness (LUFS, BS.1770 gating) of a 1-D or 2-D signal at RATE; short sounds: ungated mean"""
    data = signal if signal.ndim == 2 else signal[:, None]
    weighted = k_weight(data)
    block, hop = samples(0.4), samples(0.1)
    if len(weighted) < block:
        return float(-0.691 + 10 * np.log10(np.sum(np.mean(weighted ** 2, axis=0)) + 1e-15))
    powers = np.array([np.sum(np.mean(weighted[i:i + block] ** 2, axis=0)) for i in range(0, len(weighted) - block + 1, hop)])
    levels = -0.691 + 10 * np.log10(powers + 1e-15)
    gated = powers[levels > -70]
    if not len(gated):
        return -70.0
    relative = -0.691 + 10 * np.log10(np.mean(gated) + 1e-15) - 10
    gated = powers[levels > max(-70, relative)]
    return float(-0.691 + 10 * np.log10(np.mean(gated) + 1e-15))


def loudest_window_db(signal, window_s=0.1):
    """Level of the loudest window_s of the signal (dB RMS re full scale)"""
    flat = signal.mean(axis=1) if signal.ndim == 2 else signal
    n = samples(window_s)
    if len(flat) <= n:
        return float(db(np.sqrt(np.mean(flat ** 2))))
    energy = np.convolve(flat ** 2, np.ones(n) / n, mode="valid")
    return float(10 * np.log10(energy.max() + 1e-15))


def centroid_hz(signal):
    flat = signal.mean(axis=1) if signal.ndim == 2 else signal
    spectrum = np.abs(np.fft.rfft(flat * np.hanning(len(flat))))
    freqs = np.fft.rfftfreq(len(flat), 1.0 / RATE)
    return float(np.sum(freqs * spectrum) / max(np.sum(spectrum), 1e-12))


def normalize_peak(signal, peak_dbfs):
    peak = np.max(np.abs(signal))
    if peak < 1e-9:
        raise ValueError("Silent signal")
    return signal * (gain_db(peak_dbfs) / peak)


def seam_loop(signal, crossfade_s):
    """Overlaps the tail on the head with an equal-power crossfade and rotates the join inside: the result loops
    seamlessly (last sample flows into the first). Length shrinks by crossfade_s."""
    count = min(samples(crossfade_s), len(signal) // 4)
    if count < 2:
        raise ValueError("Loop too short")
    t = np.linspace(0.0, 1.0, count)
    fade_in, fade_out = np.sin(0.5 * np.pi * t), np.cos(0.5 * np.pi * t)
    if signal.ndim == 2:
        fade_in, fade_out = fade_in[:, None], fade_out[:, None]
    join = signal[-count:] * fade_out + signal[:count] * fade_in
    return np.concatenate((signal[count:-count], join))


def equal_power_concat(pieces, crossfade_s):
    """Joins 1-D pieces end to start with equal-power crossfades"""
    count = samples(crossfade_s)
    out = pieces[0].copy()
    t = np.linspace(0.0, 1.0, count)
    fade_in, fade_out = np.sin(0.5 * np.pi * t), np.cos(0.5 * np.pi * t)
    for piece in pieces[1:]:
        n = min(count, len(out), len(piece))
        mixed = out[-n:] * fade_out[-n:] + piece[:n] * fade_in[:n]
        out = np.concatenate((out[:-n], mixed, piece[n:]))
    return out


def place(target, signal, at_s, gain=1.0):
    """Adds signal into target (1-D) from at_s, clipped to the target's length"""
    start = samples(at_s)
    if start >= len(target) or start + len(signal) <= 0:
        return target
    begin = max(0, -start)
    end = min(len(signal), len(target) - start)
    target[start + begin:start + end] += gain * signal[begin:end]
    return target


def pitch_shift(signal, ratio):
    """Varispeed (pitch and length together, like a tape): ratio > 1 is higher and shorter"""
    if abs(ratio - 1.0) < 1e-4:
        return signal.copy()
    up, down = int(round(1000 / ratio)), 1000
    div = math.gcd(up, down)
    return resample_poly(signal, up // div, down // div, axis=0)


def room_impulse(seed, seconds_long, decay_s, lowpass_hz=6000.0, predelay_s=0.0, early=()):
    """Synthetic impulse response: exponentially decaying noise (outdoor diffuse tail), darker as it decays, with
    optional early reflections (time s, gain)"""
    rng = np.random.default_rng(seed)
    n = samples(seconds_long)
    t = np.arange(n) / RATE
    noise = rng.standard_normal(n) * np.exp(-6.91 * t / max(decay_s, 1e-3))
    bright = butter_filter(noise, "lowpass", lowpass_hz, 2)
    dark = butter_filter(noise, "lowpass", lowpass_hz * 0.25, 2)
    mix = np.clip(t / max(decay_s, 1e-3), 0, 1)
    ir = bright * (1 - mix) + dark * mix
    ir = np.concatenate((np.zeros(samples(predelay_s)), ir))
    for time_s, gain in early:
        index = samples(time_s)
        if index < len(ir):
            ir[index] += gain * np.sqrt(np.sum(ir ** 2))
    return ir / max(np.sqrt(np.sum(ir ** 2)), 1e-12)


def convolve(signal, impulse):
    return fftconvolve(signal, impulse)[: len(signal) + len(impulse) - 1]


def trim_silence(signal, threshold_db=-60.0, keep_s=0.01):
    """Cuts leading and trailing parts quieter than threshold_db below the peak (10 ms windows), keeping keep_s"""
    env, window = envelope(signal, 0.005)
    peak = env.max()
    loud = np.where(db(env / max(peak, 1e-12)) > threshold_db)[0]
    if not len(loud):
        return signal
    start = max(0, samples(loud[0] * window - keep_s))
    end = min(len(signal), samples((loud[-1] + 1) * window + keep_s))
    return signal[start:end]


def write_pcm16(path, signal):
    """48 kHz PCM16 WAV; returns the decoded check (frames, channels, peak dBFS)"""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    data = signal if signal.ndim == 2 else signal[:, None]
    if np.max(np.abs(data)) >= 1.0:
        raise ValueError(f"Clipping in {path.name}")
    sf.write(str(path), data, RATE, subtype="PCM_16")
    decoded, rate = sf.read(str(path), always_2d=True)
    assert rate == RATE and decoded.shape[1] == data.shape[1] and len(decoded) == len(data), path
    return decoded


def measures(decoded, loop=False):
    flat = decoded.mean(axis=1)
    out = {"seconds": round(len(decoded) / RATE, 4), "rate": RATE, "channels": int(decoded.shape[1]),
           "peak_dbfs": round(float(db(np.max(np.abs(decoded)))), 2), "loudest_100ms_db": round(loudest_window_db(decoded), 2),
           "loudness_lufs": round(loudness(decoded), 2), "centroid_hz": round(centroid_hz(flat), 0)}
    if loop:
        seam = float(np.max(np.abs(decoded[-1] - decoded[0])))
        ordinary = float(np.percentile(np.abs(np.diff(flat)), 99.9))
        out["loop_seam_delta"] = round(seam, 6)
        out["loop_seam_ok"] = bool(seam <= ordinary + 2 / 32768)
    else:
        out["ends_silent"] = bool(np.max(np.abs(decoded[[0, -1]])) < 4 / 32768)
    return out
