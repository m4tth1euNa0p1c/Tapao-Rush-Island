#!/usr/bin/env python3
"""Reader and writer of Tidemark ``.tmk`` folders, without Unreal (numpy only).

A ``.tmk`` is a folder: ``manifest.json`` plus one little-endian binary file per
layer (``elevation_cm.bin`` as int16, ``flags.bin`` as uint8, both row-major,
Y rows of X cells). Specification: ``Plugins/Tidemark/Docs/TMK.md``.

API::

    grid, credits = tmk.load("Island.tmk")       # verifies sha256 and source_hash
    tmk.save("Island.tmk", elevation_cm, flags, origin_cm=(0, 0), cell_size_cm=400, credits=[...])

CLI::

    python tmk.py info <dir>      dimensions, min/max/median, ASCII histogram, credits

``source_hash`` is FNV-1a 64 over the header and the two layers, byte for byte
the same value as ``FTidemarkGrid::ComputeSourceHash`` in C++.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path
from typing import Any

import numpy as np

SCHEMA_VERSION = 1
MANIFEST = "manifest.json"
ELEVATION_FILE = "elevation_cm.bin"
FLAGS_FILE = "flags.bin"
ELEVATION_LAYER = "elevation_cm"
FLAGS_LAYER = "flags"
MAX_BYTES = 16 * 1024 * 1024
MAX_CELLS = MAX_BYTES // 3
CREDIT_FIELDS = ("producer", "license", "url", "date")
GENERATOR = "Tidemark/tmk.py"

FLAG_NONE = 0
FLAG_OPEN_SEA = 1
FLAG_BORDER = 2
FLAG_FORBIDDEN = 4

FNV1A64_BASIS = 0xCBF29CE484222325
FNV1A64_PRIME = 0x100000001B3
_MASK64 = (1 << 64) - 1


class TmkError(ValueError):
    """Malformed, incomplete or corrupted .tmk folder."""


# ---------------------------------------------------------------------------
# Hashes


def fnv1a64(data: bytes, basis: int = FNV1A64_BASIS) -> int:
    """FNV-1a 64 over raw bytes; pass the previous result as ``basis`` to chain."""
    h = basis
    for b in data:
        h ^= b
        h = (h * FNV1A64_PRIME) & _MASK64
    return h


def source_hash(origin_cm: tuple[float, float], cell_size_cm: float, elevation_cm: np.ndarray, flags: np.ndarray) -> int:
    """Same value as ``FTidemarkGrid::ComputeSourceHash``: origin (2 doubles LE), cell size (float LE),
    SizeX, SizeY (int32 LE), every elevation (int16 LE, row-major), every flag byte."""
    elevation_cm = np.ascontiguousarray(elevation_cm, dtype="<i2")
    flags = np.ascontiguousarray(flags, dtype=np.uint8)
    size_y, size_x = elevation_cm.shape
    header = struct.pack("<ddfii", float(origin_cm[0]), float(origin_cm[1]), float(cell_size_cm), size_x, size_y)
    h = fnv1a64(header)
    h = fnv1a64(elevation_cm.tobytes(order="C"), h)
    h = fnv1a64(flags.tobytes(order="C"), h)
    return h


def source_hash_hex(*args: Any) -> str:
    return f"{source_hash(*args):016x}"


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


# ---------------------------------------------------------------------------
# Credits


def check_credits(credits: list[dict[str, str]]) -> None:
    """Raises TmkError unless there is at least one credit with every field non-empty."""
    if not isinstance(credits, list) or not credits:
        raise TmkError("credits[] is empty: producer, license, url and date are mandatory")
    for index, credit in enumerate(credits):
        if not isinstance(credit, dict):
            raise TmkError(f"credits[{index}] is not an object")
        for field in CREDIT_FIELDS:
            value = credit.get(field)
            if not isinstance(value, str) or not value.strip():
                raise TmkError(f"credits[{index}].{field} missing or empty")


# ---------------------------------------------------------------------------
# Save


def save(
    directory: str | Path,
    elevation_cm: np.ndarray,
    flags: np.ndarray | None,
    origin_cm: tuple[float, float],
    cell_size_cm: float,
    credits: list[dict[str, str]],
    crs: str | None = None,
) -> dict[str, Any]:
    """Writes a .tmk folder and returns its manifest. ``elevation_cm`` is a 2-D array (rows = Y, columns = X)."""
    elevation_cm = np.ascontiguousarray(elevation_cm, dtype="<i2")
    if elevation_cm.ndim != 2 or elevation_cm.size == 0:
        raise TmkError("elevation_cm must be a non-empty 2-D array (Y rows, X columns)")
    if elevation_cm.size > MAX_CELLS:
        raise TmkError(f"{elevation_cm.size} cells exceed the budget of {MAX_CELLS} cells ({MAX_BYTES} bytes)")
    if flags is None:
        flags = np.zeros(elevation_cm.shape, dtype=np.uint8)
    flags = np.ascontiguousarray(flags, dtype=np.uint8)
    if flags.shape != elevation_cm.shape:
        raise TmkError(f"flags shape {flags.shape} differs from elevation shape {elevation_cm.shape}")
    if not (float(cell_size_cm) > 0.0):
        raise TmkError("cell_size_cm must be positive")
    check_credits(credits)

    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    elevation_bytes = elevation_cm.tobytes(order="C")
    flag_bytes = flags.tobytes(order="C")
    (directory / ELEVATION_FILE).write_bytes(elevation_bytes)
    (directory / FLAGS_FILE).write_bytes(flag_bytes)

    size_y, size_x = elevation_cm.shape
    manifest: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "generator": GENERATOR,
        "origin_cm": [float(origin_cm[0]), float(origin_cm[1])],
        "cell_size_cm": float(cell_size_cm),
        "size": [int(size_x), int(size_y)],
        "crs": crs,
        "layers": [
            {"name": ELEVATION_LAYER, "type": "int16", "file": ELEVATION_FILE, "sha256": sha256_hex(elevation_bytes)},
            {"name": FLAGS_LAYER, "type": "uint8", "file": FLAGS_FILE, "sha256": sha256_hex(flag_bytes)},
        ],
        "source_hash": source_hash_hex(origin_cm, cell_size_cm, elevation_cm, flags),
        "credits": [{field: str(credit[field]) for field in CREDIT_FIELDS} for credit in credits],
    }
    (directory / MANIFEST).write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


# ---------------------------------------------------------------------------
# Load


def _layer(manifest: dict[str, Any], name: str) -> dict[str, Any]:
    for layer in manifest.get("layers", []):
        if isinstance(layer, dict) and layer.get("name") == name:
            return layer
    raise TmkError(f"manifest.json: layer `{name}` missing")


def _read_layer(directory: Path, layer: dict[str, Any], expected_type: str, dtype: str, shape: tuple[int, int]) -> np.ndarray:
    for key in ("name", "type", "file", "sha256"):
        if not isinstance(layer.get(key), str):
            raise TmkError(f"manifest.json: layer `{layer.get('name')}` lacks `{key}`")
    if layer["type"] != expected_type:
        raise TmkError(f"manifest.json: layer `{layer['name']}` has type {layer['type']}, expected {expected_type}")
    file_name = layer["file"]
    if not file_name or "/" in file_name or "\\" in file_name or ".." in file_name:
        raise TmkError(f"manifest.json: layer `{layer['name']}` has an invalid file name")
    path = directory / file_name
    if not path.is_file():
        raise TmkError(f"layer file missing: {path}")
    data = path.read_bytes()
    expected_bytes = shape[0] * shape[1] * np.dtype(dtype).itemsize
    if len(data) != expected_bytes:
        raise TmkError(f"{file_name}: {len(data)} bytes, expected {expected_bytes}")
    actual = sha256_hex(data)
    if actual != layer["sha256"].lower():
        raise TmkError(f"{file_name}: sha256 mismatch (manifest {layer['sha256']}, file {actual})")
    return np.frombuffer(data, dtype=dtype).reshape(shape).copy()


def load(directory: str | Path, verify_source_hash: bool = True) -> tuple[dict[str, Any], list[dict[str, str]]]:
    """Reads a .tmk folder. Returns ``(grid, credits)`` where grid holds ``schema_version``, ``origin_cm``,
    ``cell_size_cm``, ``size`` (x, y), ``crs``, ``elevation_cm`` (int16, shape (y, x)), ``flags`` (uint8), ``source_hash``.
    Raises TmkError on a missing manifest, a wrong schema, a wrong size or a wrong digest."""
    directory = Path(directory)
    manifest_path = directory / MANIFEST
    if not manifest_path.is_file():
        raise TmkError(f"missing {manifest_path}")
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    except ValueError as error:
        raise TmkError(f"{manifest_path}: malformed JSON ({error})") from error
    if not isinstance(manifest, dict):
        raise TmkError(f"{manifest_path}: object expected at the root")
    if manifest.get("schema_version") != SCHEMA_VERSION:
        raise TmkError(f"manifest.json: schema_version {manifest.get('schema_version')}, expected {SCHEMA_VERSION}")

    try:
        origin = (float(manifest["origin_cm"][0]), float(manifest["origin_cm"][1]))
        cell_size = float(manifest["cell_size_cm"])
        size_x, size_y = int(manifest["size"][0]), int(manifest["size"][1])
    except (KeyError, IndexError, TypeError, ValueError) as error:
        raise TmkError(f"manifest.json: origin_cm[2], cell_size_cm or size[2] missing or invalid ({error})") from error
    if size_x <= 0 or size_y <= 0 or size_x * size_y > MAX_CELLS or not (cell_size > 0.0):
        raise TmkError(f"manifest.json: size {size_x} x {size_y} or cell_size_cm {cell_size} out of range (max {MAX_CELLS} cells)")
    if not isinstance(manifest.get("layers"), list):
        raise TmkError("manifest.json: layers[] missing")

    shape = (size_y, size_x)
    elevation = _read_layer(directory, _layer(manifest, ELEVATION_LAYER), "int16", "<i2", shape)
    flags = _read_layer(directory, _layer(manifest, FLAGS_LAYER), "uint8", "u1", shape)

    stored = manifest.get("source_hash")
    if verify_source_hash and isinstance(stored, str) and stored:
        actual = source_hash_hex(origin, cell_size, elevation, flags)
        if actual != stored.lower():
            raise TmkError(f"manifest.json: source_hash {stored} does not match the layers ({actual})")

    credits = manifest.get("credits")
    if not isinstance(credits, list):
        credits = []
    grid = {
        "schema_version": SCHEMA_VERSION,
        "origin_cm": origin,
        "cell_size_cm": cell_size,
        "size": (size_x, size_y),
        "crs": manifest.get("crs"),
        "elevation_cm": elevation,
        "flags": flags,
        "source_hash": stored,
    }
    return grid, credits


# ---------------------------------------------------------------------------
# CLI


def ascii_histogram(values: np.ndarray, bins: int = 16, width: int = 40) -> list[str]:
    counts, edges = np.histogram(values, bins=bins)
    top = int(counts.max()) if counts.size else 0
    lines = []
    for count, lo, hi in zip(counts, edges[:-1], edges[1:]):
        bar = "#" * (int(count) * width // top if top else 0)
        lines.append(f"{lo / 100:9.2f} .. {hi / 100:9.2f} m |{bar:<{width}}| {int(count)}")
    return lines


def info(directory: str | Path) -> str:
    grid, credits = load(directory)
    elevation = grid["elevation_cm"]
    flags = grid["flags"]
    size_x, size_y = grid["size"]
    lines = [
        f"tmk: {directory}",
        f"schema_version: {grid['schema_version']}",
        f"size: {size_x} x {size_y} cells, cell {grid['cell_size_cm']:g} cm, extent {size_x * grid['cell_size_cm'] / 100:.1f} x {size_y * grid['cell_size_cm'] / 100:.1f} m",
        f"origin_cm: {grid['origin_cm'][0]:g}, {grid['origin_cm'][1]:g}   crs: {grid['crs'] or 'none'}",
        f"bytes: {elevation.nbytes + flags.nbytes} (budget {MAX_BYTES})",
        f"elevation_cm: min {int(elevation.min())}  median {float(np.median(elevation)):.0f}  max {int(elevation.max())}",
        f"flags: open_sea {int((flags & FLAG_OPEN_SEA != 0).sum())}  border {int((flags & FLAG_BORDER != 0).sum())}  forbidden {int((flags & FLAG_FORBIDDEN != 0).sum())}",
        f"source_hash: {grid['source_hash']} (verified)",
        "histogram:",
        *("  " + line for line in ascii_histogram(elevation)),
        "credits:",
    ]
    for credit in credits:
        lines.append("  - " + " | ".join(f"{field}: {credit.get(field, '')}" for field in CREDIT_FIELDS))
    if not credits:
        lines.append("  (none: the CI refuses this manifest)")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    p_info = sub.add_parser("info", help="describe a .tmk folder")
    p_info.add_argument("dir", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "info":
            print(info(args.dir))
    except TmkError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
