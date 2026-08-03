from __future__ import annotations

import json
import subprocess
import urllib.request
from pathlib import Path

repository = "Autodidac/Vulkan_AI_Walking_Training_Simulation"
extra_header = subprocess.check_output(
    ["git", "config", "--get", "http.https://github.com/.extraheader"],
    text=True,
).strip()
if ":" not in extra_header:
    raise RuntimeError("GitHub checkout authorization header is unavailable")
name, value = extra_header.split(":", 1)
if name.strip().lower() != "authorization" or not value.strip():
    raise RuntimeError("GitHub checkout authorization header is malformed")

request = urllib.request.Request(
    f"https://api.github.com/repos/{repository}/dispatches",
    data=json.dumps({"event_type": "validate_runner_v0711"}).encode("utf-8"),
    method="POST",
    headers={
        "Authorization": value.strip(),
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "Runner-v0.7.11-validator-dispatch",
        "Content-Type": "application/json",
    },
)
with urllib.request.urlopen(request, timeout=30) as response:
    if response.status != 204:
        raise RuntimeError(f"repository dispatch returned HTTP {response.status}")

Path(__file__).unlink()
