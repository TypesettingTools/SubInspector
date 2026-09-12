"""The example accepts ASS files with either newline style, with or without a BOM."""

import subprocess
import sys
import tempfile
from pathlib import Path

script = """[Script Info]
ScriptType: v4.00+
PlayResX: 640
PlayResY: 480
[V4+ Styles]
Style: Default,Arial,20,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,0,0,7,0,0,0,1
[Events]
Dialogue: 0,0:00:00.00,0:00:02.00,Default,,0,0,0,,{\\an7\\pos(100,100)\\p1}m 0 0 l 20 0 20 20 0 20
"""

with tempfile.TemporaryDirectory(prefix="SubInspector example ") as directory:
    path = Path(directory) / "sample.ass"
    for newline in ("\n", "\r\n"):
        for bom in (b"", b"\xef\xbb\xbf"):
            path.write_bytes(bom + script.replace("\n", newline).encode("utf-8"))
            result = subprocess.run([sys.argv[1], str(path)], capture_output=True, text=True, check=True)
            assert "Deleted 0 lines out of 1" in result.stdout, result.stdout
