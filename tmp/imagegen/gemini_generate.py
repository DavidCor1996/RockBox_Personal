#!/usr/bin/env python3

from __future__ import annotations

import argparse
import base64
from pathlib import Path

from google import genai


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", required=True)
    parser.add_argument("--prompt", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    client = genai.Client()
    response = client.models.generate_content(
        model=args.model,
        contents=args.prompt,
    )

    saved = 0

    for candidate in getattr(response, "candidates", []) or []:
        content = getattr(candidate, "content", None)
        parts = getattr(content, "parts", None) or []
        for index, part in enumerate(parts):
            inline_data = getattr(part, "inline_data", None)
            if inline_data is None:
                continue
            data = getattr(inline_data, "data", None)
            mime_type = getattr(inline_data, "mime_type", "")
            if not data:
                continue
            raw = base64.b64decode(data) if isinstance(data, str) else data
            suffix = ".png" if "png" in mime_type else ".jpg"
            target = out_path if saved == 0 else out_path.with_name(f"{out_path.stem}-{saved + 1}{suffix}")
            if target.suffix != suffix:
                target = target.with_suffix(suffix)
            target.write_bytes(raw)
            print(f"Wrote {target}")
            saved += 1

    if saved == 0:
        text = getattr(response, "text", None)
        if text:
            print(text)
        raise SystemExit("No image returned")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
