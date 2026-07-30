#!/usr/bin/env python3
"""Run a Live TV sync from the command line.

Converts every show and commercial with the current encode profile and
copies them to a mounted iPod together with the channel table and guide.
Useful when a profile change means everything has to be rebuilt and you
would rather watch it run in a terminal than in the UI.

Usage: livetv_sync_cli.py "/run/media/<user>/<IPOD>"
"""

import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from app.config import Config  # noqa: E402
from services.livetv import (  # noqa: E402
    LiveTvLibrary,
    LiveTvLineup,
    LiveTvSync,
    ensure_weather_channel,
)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    mount = sys.argv[1]
    if not os.path.isdir(mount):
        print(f"Not a directory: {mount}")
        return 1

    config = Config()
    library = LiveTvLibrary(config)
    sync = LiveTvSync(library)

    print("Scanning Live TV folders...", flush=True)
    shows, ads = library.scan(probe_durations=True)
    print(f"  {len(shows)} shows, {len(ads)} commercials", flush=True)

    lineup = LiveTvLineup(library)
    if not lineup.channels:
        lineup.autobuild(shows, ads)
        lineup.save()

    before = len(shows)
    shows = ensure_weather_channel(sync, lineup, config, shows, ads)
    if len(shows) > before:
        print(f"  Weather channel: {len(shows) - before} bumper(s)",
              flush=True)
    lineup.save()
    print(f"  {len(lineup.channels)} channels", flush=True)

    started = time.time()
    state = {"last": 0.0}

    def progress(done, total, label):
        now = time.time()
        if now - state["last"] < 2 and done != total:
            return
        state["last"] = now
        elapsed = now - started
        rate = done / elapsed if elapsed > 0 else 0
        remaining = (total - done) / rate if rate > 0 else 0
        print(f"  [{done}/{total}] {int(elapsed)}s elapsed, "
              f"~{int(remaining)}s left  {label[:56]}", flush=True)

    summary = sync.sync(mount, lineup, shows, ads, progress=progress)

    print(f"\nConverted {summary['converted']}, copied {summary['copied']}, "
          f"unchanged {summary['skipped']}")
    print(f"{summary['channels']} channels, {summary['slots']} listings")
    if summary.get("cache_pruned"):
        print(f"Removed {summary['cache_pruned']} unused cached clip(s)")
    for warning in summary.get("warnings") or []:
        print(f"WARNING: {warning}")
    for error in (summary.get("errors") or [])[:10]:
        print(f"ERROR: {error}")
    os.system("sync")
    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
