import os
import threading
from http.server import ThreadingHTTPServer

from app.config import Config
from scripts.music_share_relay import RelayStore, make_handler
from services.music_sharing import MusicSharingService, share_item_to_store_result


def _sharing_config(path, name):
    cfg = Config(os.path.join(path, f"{name}.json"))
    cfg.cache_dir = os.path.join(path, name, "cache")
    cfg.artwork_cache_dir = os.path.join(path, name, "cache", "artwork")
    cfg.db_path = os.path.join(path, name, "library.db")
    cfg.ensure_dirs()
    return cfg


def test_music_sharing_relay_round_trip(tmp_dir):
    store = RelayStore(os.path.join(tmp_dir, "relay.json"))
    server = ThreadingHTTPServer(("127.0.0.1", 0), make_handler(store))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        relay_url = f"http://127.0.0.1:{server.server_port}"
        sender = MusicSharingService(_sharing_config(tmp_dir, "sender"))
        receiver = MusicSharingService(_sharing_config(tmp_dir, "receiver"))
        sender.set_settings(relay_url=relay_url, pair_code="pair-test", display_name="Sender")
        receiver.set_settings(relay_url=relay_url, pair_code="pair-test", display_name="Receiver")

        sender.send(
            {
                "kind": "album",
                "title": "Shared Album",
                "artist": "Shared Artist",
                "url": "https://tidal.com/album/123",
                "note": "listen",
            }
        )
        added = receiver.fetch()

        inbox, outbox = receiver.history()
        assert added == 1
        assert outbox == []
        assert inbox[0]["sender_name"] == "Sender"
        assert inbox[0]["item"]["title"] == "Shared Album"
        result = share_item_to_store_result(inbox[0]["item"])
        assert result["source"] == "tidal"
        assert result["media_type"] == "album"
        assert result["id"] == "123"
        assert result["url"] == "https://tidal.com/album/123"
    finally:
        server.shutdown()
        thread.join(timeout=2)
