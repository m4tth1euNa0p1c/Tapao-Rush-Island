"""Validate prepared audio and the review catalogue without touching Unreal."""
import hashlib
import json
from datetime import datetime, timezone
from html.parser import HTMLParser
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    manifest_path = ROOT / "art/audio/expedition/manifest.json"
    manifest = json.loads(manifest_path.read_text("utf-8"))
    catalogue = json.loads((ROOT / "docs/audio/expedition-audio-catalogue.json").read_text("utf-8"))
    ids, paths, results = set(), set(), []
    for entry in manifest["sounds"]:
        assert entry["id"] not in ids and entry["asset"] not in paths, "Duplicate sound/asset"
        ids.add(entry["id"])
        paths.add(entry["asset"])
        source, prepared = ROOT / entry["source_file"], ROOT / entry["file"]
        assert digest(source) == entry["source_sha256"], entry["id"]
        assert digest(prepared) == entry["sha256"], entry["id"]
        audio, rate = sf.read(prepared, always_2d=True)
        info = sf.info(prepared)
        assert info.format == "WAV" and info.subtype == "PCM_16"
        assert rate == 48000 and audio.shape[1] in (1, 2)
        assert np.isfinite(audio).all()
        assert abs(len(audio) / rate - entry["seconds"]) < .0001
        assert np.max(np.abs(audio)) < .51, "Clipping/headroom regression"
        assert np.sqrt(np.mean(audio**2)) > .00001, "Silent recording"
        if not entry["loop"]:
            assert np.max(np.abs(audio[[0, -1]])) < 1 / 32768, "Missing one-shot fade"
        else:
            seam = np.max(np.abs(audio[-1] - audio[0]))
            ordinary = np.max(np.abs(np.diff(audio, axis=0)))
            assert seam <= ordinary + 1/32768, "Loop boundary spike"
        assert entry["license"] == "CC0-1.0" and entry["source_url"].startswith("https://")
        results.append({"id":entry["id"], "sha256":entry["sha256"], "passed":True})
    assert len(results) == 76
    events = catalogue["events"]
    assert len(events) == len({e["id"] for e in events}) == 138
    assert len({e["category"] for e in events}) == 10
    for event in events:
        if event["audio"]:
            local = (ROOT / "docs/audio" / event["audio"]).resolve()
            assert local.is_relative_to(ROOT) and local.is_file(), event["id"]
            assert event["sound_id"] in ids
        assert event["source"]["title"] and event["source"]["license"]
    page = (ROOT / "docs/audio/expedition-audio.html").read_text("utf-8")
    HTMLParser().feed(page)
    assert "/*CATALOGUE_DATA*/" not in page
    assert 'lang="fr"' in page and '<audio id="audio"' in page
    # This permits a JS parser check separately; it does not claim a browser render.
    script = page.split("<script>", 1)[1].split("</script>", 1)[0]
    saved = ROOT / "monke_island/Saved/T234"
    saved.mkdir(parents=True, exist_ok=True)
    (saved / "catalogue-script.js").write_text(script, encoding="utf-8")
    report = {"at":datetime.now(timezone.utc).isoformat(), "success":True,
              "level":"offline audio data and catalogue integrity only", "manifest_sha256":digest(manifest_path),
              "sounds":results, "events":138, "categories":10, "browser_render_verified":False,
              "unreal_import_verified":False, "pie_listened":False, "packaged_tested":False, "multiplayer_tested":False}
    (ROOT / "docs/audio/expedition-audio-validation.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print("PASS: 76 decoded WAVs, source/output hashes, PCM16/48kHz, headroom, fades/loop joins, 138 events, local audio links")
    print("NOT TESTED: browser rendering, Unreal import, audible PIE, packaged game, multiplayer")


if __name__ == "__main__":
    main()
