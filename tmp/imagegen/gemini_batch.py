#!/usr/bin/env python3

from __future__ import annotations

import argparse
import base64
import json
import time
from pathlib import Path

from google import genai


def build_prompt(job: dict) -> str:
    sections = []
    if job.get("use_case"):
        sections.append(f"Use case: {job['use_case']}")
    sections.append(f"Primary request: {job['prompt']}")
    if job.get("style"):
        sections.append(f"Style/medium: {job['style']}")
    if job.get("composition"):
        sections.append(f"Composition/framing: {job['composition']}")
    if job.get("lighting"):
        sections.append(f"Lighting/mood: {job['lighting']}")
    if job.get("palette"):
        sections.append(f"Color palette: {job['palette']}")
    if job.get("constraints"):
        sections.append(f"Constraints: {job['constraints']}")
    if job.get("negative"):
        sections.append(f"Avoid: {job['negative']}")
    return "\n".join(sections)


def save_response(response, out_path: Path) -> int:
    for part in getattr(response, "parts", []) or []:
        inline_data = getattr(part, "inline_data", None)
        if inline_data is None:
            continue
        data = getattr(inline_data, "data", None)
        mime_type = getattr(inline_data, "mime_type", "")
        if not data:
            continue
        raw = base64.b64decode(data) if isinstance(data, str) else data
        suffix = ".png" if "png" in mime_type else ".jpg"
        target = out_path
        if target.suffix != suffix:
            target = target.with_suffix(suffix)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(raw)
        print(f"Wrote {target}")
        return 1
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", default="gemini-2.5-flash-image")
    parser.add_argument("--input", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--start-at", type=int, default=1)
    parser.add_argument("--max-attempts", type=int, default=4)
    args = parser.parse_args()

    client = genai.Client()
    out_dir = Path(args.out_dir)
    jobs = [json.loads(line) for line in Path(args.input).read_text(encoding="utf-8").splitlines() if line.strip()]

    for index, job in enumerate(jobs, start=1):
        if index < args.start_at:
            continue
        out_name = job.get("out") or f"image_{index}.png"
        prompt = build_prompt(job)
        print(f"[job {index}/{len(jobs)}] {out_name}")
        for attempt in range(1, args.max_attempts + 1):
            response = client.models.generate_content(
                model=args.model,
                contents=[prompt],
            )
            saved = save_response(response, out_dir / out_name)
            if saved:
                break
            text = getattr(response, "text", None)
            if text:
                print(f"[job {index}] attempt {attempt}: {text}")
            if attempt == args.max_attempts:
                raise SystemExit(f"No image returned for job {index}")
            time.sleep(2)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
