"""T-233: download the explicitly selected CC0 audio sources, never execute archives.

Source pages and archives stay in ignored art/audio_src/expedition. Only audio is
extracted. The inventory retains download URLs, sizes and SHA-256 for reproducibility.
No Unreal editor, OneDrive, C++ or existing game sound is touched.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
from datetime import datetime, timezone
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urljoin, urlparse
import zipfile

import requests

ROOT = Path(__file__).resolve().parents[2]
SOURCE_FILE = Path(__file__).with_name("expedition_sources.json")
OUT = ROOT / "art/audio_src/expedition"
EXTENSIONS = {".wav", ".ogg", ".flac", ".mp3"}
LIMIT = 64 * 1024 * 1024
TOTAL_LIMIT = 512 * 1024 * 1024


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


def fetch(url, target, session):
    if target.exists():
        data = target.read_bytes()
    else:
        with session.get(url, timeout=(15, 60), stream=True) as response:
            response.raise_for_status()
            pieces, size = [], 0
            for piece in response.iter_content(65536):
                size += len(piece)
                if size > LIMIT:
                    raise ValueError(f"Source exceeds {LIMIT} bytes: {url}")
                pieces.append(piece)
            data = b"".join(pieces)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    return data


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--source", action="append", default=[])
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    session = requests.Session()
    session.headers["User-Agent"] = "CrownfallAudioAssetResearch/1.0"
    inventory_path = OUT / "inventory.json"
    inventory = json.loads(inventory_path.read_text("utf-8")) if inventory_path.exists() else {}
    for source in json.loads(SOURCE_FILE.read_text("utf-8"))["sources"]:
        key, page = source["id"], source["page"]
        if args.source and key not in args.source:
            continue
        folder = OUT / key
        html = fetch(page, folder / "source.html", session).decode("utf-8")
        if "creativecommons.org/publicdomain/zero" not in html:
            raise ValueError(f"CC0 licence link missing; inspect manually: {page}")
        links = Links()
        links.feed(html)
        if urlparse(page).netloc == "opengameart.org":
            urls = [urljoin(page, url) for url in links.urls
                    if "/sites/default/files/" in url and Path(urlparse(url).path).suffix.lower() in EXTENSIONS | {".zip"}]
        else:
            urls = [urljoin(page, url) for url in links.urls
                    if "/media/pages/assets/" in url and urlparse(url).path.endswith(".zip")]
        urls = list(dict.fromkeys(urls))
        if not urls:
            raise ValueError(f"No public download links: {page}")
        files, downloads, total = [], [], 0
        for index, url in enumerate(urls):
            name = Path(unquote(urlparse(url).path)).name
            if not Path(name).suffix:
                name = f"download_{index}.zip"
            data = fetch(url, folder / "downloads" / name, session)
            downloads.append({"url": url, "file": name, "bytes": len(data), "sha256": digest(data)})
            contents = []
            if zipfile.is_zipfile(io.BytesIO(data)):
                with zipfile.ZipFile(io.BytesIO(data)) as archive:
                    for item in archive.infolist():
                        path = Path(item.filename.replace("\\", "/"))
                        if path.suffix.lower() not in EXTENSIONS or item.is_dir():
                            continue
                        if path.is_absolute() or ".." in path.parts or ":" in str(path):
                            raise ValueError(f"Unsafe archive path: {item.filename}")
                        total += item.file_size
                        if item.file_size > LIMIT or total > TOTAL_LIMIT:
                            raise ValueError("Archive exceeds extraction budget")
                        contents.append((path, archive.read(item)))
            else:
                if Path(name).suffix.lower() not in EXTENSIONS:
                    raise ValueError(f"Unsupported download: {url}")
                total += len(data)
                contents.append((Path(name), data))
            for relpath, audio in contents:
                dest = folder / "audio" / relpath
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(audio)
                files.append({"file": dest.relative_to(ROOT).as_posix(), "bytes": len(audio), "sha256": digest(audio)})
        inventory[key] = {**source, "fetched_at": datetime.now(timezone.utc).isoformat(),
                          "source_page_sha256": digest(html.encode("utf-8")), "downloads": downloads, "files": files}
        inventory_path.write_text(json.dumps(inventory, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"{key}: {len(files)} audio files, {total / 1048576:.1f} MiB", flush=True)


if __name__ == "__main__":
    main()
