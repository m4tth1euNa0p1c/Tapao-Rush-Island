"""Proof images of T-203 (weather) and T-162 (blood of each species): asset thumbnails rendered by the editor.

Outside the editor, through the Unreal MCP (HTTP JSON-RPC, EditorAppToolset.CaptureAssetImage), after the actions
"stormartifact" and "bloodtint" built the assets. Read only: renders thumbnails, writes nothing in the project content.
Takes the lead session's action lock (monke_island/Saved/T186/action.lockdir) so it never runs during an action.

Usage: python scripts/editor/weather_blood_captures.py
Writes docs/production/captures/T-203/*.png and docs/production/captures/T-162/*.png, and a summary on stdout.
"""
import base64
import json
import re
import sys
import time
import urllib.request
from pathlib import Path

URL = "http://127.0.0.1:8000/mcp"
HDR = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
ROOT = Path(__file__).resolve().parents[2]
LOCK = ROOT / "monke_island/Saved/T186/action.lockdir"
ASSETS = {
    "T-203": ["/Game/MonkeIsland/Environment/Weather/M_Rain_Streak", "/Game/MonkeIsland/Environment/Weather/NS_Rain"],
    "T-162": [f"/Game/MonkeIsland/VFX/Blood/MI_Blood_Particle_{blood}" for blood in ("Sap", "Hemolymph", "Lava")]
             + ["/Game/MonkeIsland/VFX/Blood/M_Blood_Particle"]
             + [f"/Game/MonkeIsland/VFX/Blood/NS_Blood_MusketImpact_{blood}" for blood in ("Sap", "Hemolymph", "Lava")]
             + ["/Game/MonkeIsland/VFX/Blood/NS_Blood_MusketImpact"],
}


class Mcp:
    def __init__(self):
        self.sid = None
        self.sid, _ = self._post({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
            "protocolVersion": "2025-03-26", "capabilities": {}, "clientInfo": {"name": "t203-t162-captures", "version": "1"}}})
        self._post({"jsonrpc": "2.0", "method": "notifications/initialized"})

    def _post(self, body, timeout=180):
        headers = dict(HDR)
        if self.sid:
            headers["Mcp-Session-Id"] = self.sid
        request = urllib.request.Request(URL, json.dumps(body).encode(), headers)
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return response.headers.get("Mcp-Session-Id") or self.sid, response.read().decode("utf-8", "replace")

    def raw(self, toolset, tool, args):
        _, text = self._post({"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {
            "name": "call_tool", "arguments": {"toolset_name": toolset, "tool_name": tool, "arguments": args}}})
        return text


def image_data(text):
    """Base64 PNG of an MCP tool answer: an image content block, or a returnValue (object or JSON string) with data"""
    if not text.startswith("{"):
        lines = [line[5:].strip() for line in text.splitlines() if line.startswith("data:")]
        text = lines[-1] if lines else text
    try:
        body = json.loads(text)
    except json.JSONDecodeError:
        match = re.search(r'"data"\s*:\s*"([A-Za-z0-9+/=]+)"', text)
        return match.group(1) if match else None
    for block in body.get("result", {}).get("content", []):
        if block.get("type") == "image" and block.get("data"):
            return block["data"]
        if block.get("type") == "text":
            try:
                value = json.loads(block.get("text", ""))
            except json.JSONDecodeError:
                continue
            value = value.get("returnValue", value) if isinstance(value, dict) else value
            if isinstance(value, str):
                try:
                    value = json.loads(value)
                except json.JSONDecodeError:
                    continue
            if isinstance(value, dict) and value.get("data"):
                return value["data"]
    return None


def main():
    for _ in range(600):
        try:
            LOCK.mkdir()
            break
        except FileExistsError:
            time.sleep(1)
    else:
        sys.exit("action lock busy for 10 min")
    try:
        mcp = Mcp()
        for task, paths in ASSETS.items():
            out = ROOT / "docs/production/captures" / task
            out.mkdir(parents=True, exist_ok=True)
            for path in paths:
                text = mcp.raw("EditorToolset.EditorAppToolset", "CaptureAssetImage", {"assetPath": path})
                data = image_data(text)
                if not data:
                    print("NO IMAGE", path, text[:300])
                    continue
                target = out / (path.rsplit("/", 1)[-1] + ".png")
                target.write_bytes(base64.b64decode(data))
                print("captured", target.relative_to(ROOT), len(data) * 3 // 4, "bytes")
    finally:
        LOCK.rmdir()


if __name__ == "__main__":
    main()
