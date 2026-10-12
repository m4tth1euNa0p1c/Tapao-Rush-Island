"""T-259: downloads the CC0 sources of the weapons v2 sounds (scripts/audio/weapons_v2_sources.json), never executes them.

Pages, archives and audio stay in art/audio_src/weapons_v2/<id>/ (ignored by Git). Only audio files are extracted from
the zip archives (paths checked, size budget). The licence is checked on each page before any download:
  OpenGameArt  the licence field of the page reads CC0;
  Kenney       the page links the CC0 deed;
  Freesound    the only Creative Commons deed the page links is CC0 1.0; the high quality public preview (MP3) is
               downloaded (the original needs an account), its URL kept.
inventory.json keeps for every file: URL, size and SHA-256 (the prepare script refuses a changed source).

  python -I scripts/audio/weapons_v2_fetch.py [--source ID ...]
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import re
import time
import zipfile
from datetime import datetime, timezone
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urljoin, urlparse

import requests

ROOT = Path(__file__).resolve().parents[2]
SOURCE_FILE = Path(__file__).with_name("weapons_v2_sources.json")
OUT = ROOT / "art/audio_src/weapons_v2"
EXTENSIONS = {".wav", ".ogg", ".flac", ".mp3"}
LIMIT = 64 * 1024 * 1024
TOTAL_LIMIT = 256 * 1024 * 1024
CC0_DEED = "creativecommons.org/publicdomain/zero/1.0"


class Links(HTMLParser):
    def __init__(self):
        super().__init__()
        self.urls = []

    def handle_starttag(self, tag, attrs):
        if tag == "a":
            href = dict(attrs).get("href")
            if href:
                self.urls.append(href)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def fetch(url, target, session, polite=0.0):
    if target.exists():
        return target.read_bytes()
    for attempt in range(6):
        with session.get(url, timeout=(15, 90), stream=True) as response:
            if response.status_code == 429:
                time.sleep(10 * (attempt + 1))
                continue
            response.raise_for_status()
            pieces, size = [], 0
            for piece in response.iter_content(65536):
                size += len(piece)
                if size > LIMIT:
                    raise ValueError(f"Source exceeds {LIMIT} bytes: {url}")
                pieces.append(piece)
            data = b"".join(pieces)
            break
    else:
        raise RuntimeError(f"Rate limited: {url}")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)
    time.sleep(polite)
    return data


def check_licence(kind, html):
    if kind == "opengameart":
        field = re.search(r"field-name-field-art-licenses(.*?)field-name-", html, re.S)
        names = re.findall(r"class='license-name'>([^<]+)<", field.group(1)) if field else []
        if names != ["CC0"]:
            raise ValueError(f"OpenGameArt licence is {names}, not CC0 only")
    elif kind == "kenney":
        if CC0_DEED not in html:
            raise ValueError("Kenney page does not link the CC0 deed")
    elif kind == "freesound":
        deeds = set(re.findall(r"creativecommons\.org/(?:licenses|publicdomain)/[a-z\-]+/[\d.]+", html))
        if deeds != {CC0_DEED}:
            raise ValueError(f"Freesound deeds {sorted(deeds)}, not CC0 only")
    else:
        raise ValueError(f"Unknown source kind {kind}")


def download_urls(kind, page, html):
    links = Links()
    links.feed(html)
    if kind == "opengameart":
        urls = [urljoin(page, u) for u in links.urls
                if "/sites/default/files/" in u and Path(urlparse(u).path).suffix.lower() in EXTENSIONS | {".zip"}]
    elif kind == "kenney":
        urls = [urljoin(page, u) for u in links.urls if "/media/pages/assets/" in u and urlparse(u).path.endswith(".zip")]
    else:
        previews = sorted(set(re.findall(r"https://cdn\.freesound\.org/previews/[^\"'\s]+-hq\.mp3", html)))
        urls = previews[:1]
    return list(dict.fromkeys(urls))


def extract(name, data, budget):
    contents = []
    if zipfile.is_zipfile(io.BytesIO(data)):
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            for item in archive.infolist():
                path = Path(item.filename.replace("\\", "/"))
                if item.is_dir() or path.suffix.lower() not in EXTENSIONS or "__MACOSX" in path.parts:
                    continue
                if path.is_absolute() or ".." in path.parts or ":" in str(path):
                    raise ValueError(f"Unsafe archive path: {item.filename}")
                budget[0] += item.file_size
                if item.file_size > LIMIT or budget[0] > TOTAL_LIMIT:
                    raise ValueError("Archive exceeds extraction budget")
                contents.append((path, archive.read(item)))
    else:
        if Path(name).suffix.lower() not in EXTENSIONS:
            raise ValueError(f"Unsupported download: {name}")
        budget[0] += len(data)
        contents.append((Path(name), data))
    return contents


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", action="append", default=[])
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    session = requests.Session()
    session.headers["User-Agent"] = "Mozilla/5.0 CrownfallAudioAssetResearch/1.0 (T-259, CC0 sound sourcing)"
    inventory_path = OUT / "inventory.json"
    inventory = json.loads(inventory_path.read_text("utf-8")) if inventory_path.exists() else {}
    for source in json.loads(SOURCE_FILE.read_text("utf-8"))["sources"]:
        key, page, kind = source["id"], source["page"], source["kind"]
        if args.source and key not in args.source:
            continue
        folder = OUT / key
        polite = 4.0 if kind == "freesound" else 0.5
        html = fetch(page, folder / "source.html", session, polite).decode("utf-8", errors="replace")
        check_licence(kind, html)
        urls = download_urls(kind, page, html)
        if not urls:
            raise ValueError(f"No public download links: {page}")
        files, downloads, budget = [], [], [0]
        for index, url in enumerate(urls):
            name = Path(unquote(urlparse(url).path)).name or f"download_{index}.zip"
            data = fetch(url, folder / "downloads" / name, session, polite)
            downloads.append({"url": url, "file": name, "bytes": len(data), "sha256": digest(data)})
            for relpath, audio in extract(name, data, budget):
                dest = folder / "audio" / relpath
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(audio)
                files.append({"file": dest.relative_to(ROOT).as_posix(), "bytes": len(audio), "sha256": digest(audio)})
        inventory[key] = {**source, "fetched_at": datetime.now(timezone.utc).isoformat(),
                          "source_page_sha256": digest(html.encode("utf-8")), "downloads": downloads, "files": files}
        inventory_path.write_text(json.dumps(inventory, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"{key}: {len(files)} audio files, {budget[0] / 1048576:.1f} MiB", flush=True)


if __name__ == "__main__":
    main()
