"""Prepare game-owned Vortex music for bounded, codec-free device streaming."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def prepare(metadata: Path) -> dict:
    fields = dict(line.split("=", 1) for line in metadata.read_text().splitlines()
                  if "=" in line)
    if fields.get("guid") != "12345" or fields.get("build_id") != "2563290":
        raise ValueError("music preparation requires the supported Vortex build")
    root = metadata.parent / "assets"
    records = []
    for name in ("a.m4a", "b.m4a", "c.m4a"):
        source = root / name
        destination = root / (name + ".pcm")
        temporary = destination.with_suffix(".pcm.tmp")
        try:
            subprocess.run([
                "ffmpeg", "-nostdin", "-v", "error", "-y", "-i", str(source),
                "-map", "0:a:0", "-vn", "-ar", "44100", "-ac", "2",
                "-c:a", "pcm_s16le", "-f", "s16le", str(temporary),
            ], check=True)
            size = temporary.stat().st_size
            if not size or size % 4:
                raise ValueError(f"invalid stereo PCM output for {name}")
            records.append({
                "source": name,
                "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                "pcm_sha256": hashlib.sha256(temporary.read_bytes()).hexdigest(),
                "frames": size // 4,
            })
            temporary.replace(destination)
        finally:
            temporary.unlink(missing_ok=True)
    report = {"version": 1, "rate": 44100, "channels": 2,
              "format": "s16le", "tracks": records}
    manifest = root / "music-pcm.json"
    temporary = manifest.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(report, indent=2) + "\n")
    temporary.replace(manifest)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("metadata", type=Path)
    args = parser.parse_args()
    print(json.dumps(prepare(args.metadata), indent=2))
