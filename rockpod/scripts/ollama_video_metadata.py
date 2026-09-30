#!/usr/bin/env python3
"""Run a bounded Ollama-guided metadata pass for RockPod videos.

The default is a five-title dry run. Use ``--apply`` after reviewing the
suggestions. ``--all`` is intentionally explicit and is not used by the test
suite or by the first-run UI action.
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from app.config import Config  # noqa: E402
from app.database import Database  # noqa: E402
from services.ollama_video_metadata import (  # noqa: E402
    DEFAULT_SAMPLE_LIMIT,
    OllamaVideoMetadataError,
    OllamaVideoMetadataRunner,
)


def _parser():
    parser = argparse.ArgumentParser(
        description="Use local Ollama parsing plus provider-grounded video metadata."
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "--limit",
        type=int,
        default=DEFAULT_SAMPLE_LIMIT,
        help="maximum number of movie/show targets (default: 5)",
    )
    mode.add_argument(
        "--all",
        action="store_true",
        help="review all eligible movie and TV-show targets",
    )
    parser.add_argument(
        "--apply",
        action="store_true",
        help="write the provider-grounded updates and catalog",
    )
    parser.add_argument(
        "--no-artwork",
        action="store_true",
        help="do not download posters, season covers, or banners",
    )
    parser.add_argument("--db", default="", help="override the configured library database")
    return parser


def main(argv=None):
    args = _parser().parse_args(argv)
    if args.limit < 1 and not args.all:
        _parser().error("--limit must be at least 1")

    config = Config()
    if not config.get("ollama_video_metadata_enabled", False):
        print(
            "Ollama video metadata is disabled. Enable it in RockPod "
            "Preferences > Metadata first.",
            file=sys.stderr,
        )
        return 2

    database = Database(args.db or config.db_path)
    try:
        rows = [
            dict(row)
            for row in database.get_tracks_by_media_type("video", order_by="id")
        ]
        runner = OllamaVideoMetadataRunner(
            config=config,
            dry_run=not args.apply,
        )
        result = runner.plan(
            rows,
            limit=None if args.all else args.limit,
            fetch_artwork=not args.no_artwork,
        )

        print(
            f"Reviewed {result['processed']} target(s) from "
            f"{result['eligible']} eligible video row(s)."
        )
        for item in result.get("suggestions") or []:
            print(
                f"  {item['label'][:48]:48} -> {item['title'] or item['search_query']} "
                f"({item['media_type']}, confidence={item['confidence']:.2f})"
            )
        for item in result.get("errors") or []:
            print(f"  ! {item['label']}: {item['error']}")
        print(f"Planned database updates: {len(result.get('updates') or [])}")

        if not args.apply:
            print("Dry run: nothing was written. Re-run with --apply to commit these updates.")
            return 0

        with database.transaction():
            for update in result.get("updates") or []:
                values = {
                    key: value
                    for key, value in (update.get("values") or {}).items()
                    if key != "metadata_hash"
                }
                database.update_track_metadata(update.get("id"), values)
        runner.save_catalog()
        print("Applied updates and saved the video artwork catalog.")
        return 0
    except OllamaVideoMetadataError as exc:
        print(f"Ollama metadata failed: {exc}", file=sys.stderr)
        return 1
    finally:
        database.close()


if __name__ == "__main__":
    raise SystemExit(main())
