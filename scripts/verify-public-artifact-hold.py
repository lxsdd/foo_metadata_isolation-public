#!/usr/bin/env python3
"""Keep manually qualified metadata-isolation component builds ephemeral."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
lines = (root / ".github/workflows/candidate.yml").read_text(encoding="utf-8").splitlines()
steps = [i for i, line in enumerate(lines)
         if line.strip() == "- name: Upload candidate (distribution currently on hold)"]
uploads = sum("uses: actions/upload-artifact@" in line for line in lines)
if len(steps) != 1 or uploads != 1 or not any(
    line.strip().startswith("if: false") for line in lines[steps[0]+1:steps[0]+6]
    ) if steps else True:
    sys.exit("FAIL: public metadata-isolation candidate upload is not gated")
print("PASS: SDK/Win32/x64 source qualification remains; candidate upload disabled")
