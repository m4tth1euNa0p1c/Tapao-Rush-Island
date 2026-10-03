"""Extrait l'image base64 d'une reponse MCP Unreal (CaptureViewport, CaptureAssetImage...) vers un PNG.

Usage: python scripts/mcp_image.py <reponse_mcp.txt> <sortie.png>
"""
import base64
import json
import re
import sys
from pathlib import Path

raw = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
match = re.search(r'"data"\s*:\s*"([A-Za-z0-9+/=\n]+)"', raw)
if not match:
    sys.exit("aucune image base64 trouvee")
data = match.group(1).replace("\n", "")
Path(sys.argv[2]).write_bytes(base64.b64decode(data))
meta = re.search(r'"cameraLocation".{0,200}', raw)
print(sys.argv[2], "|", meta.group(0)[:200] if meta else "")
