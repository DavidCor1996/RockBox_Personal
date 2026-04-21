import json
import os

from app.config import Config


def test_config_loads_legacy_single_video_dir_into_video_dirs(tmp_dir):
    cfg_path = os.path.join(tmp_dir, "config.json")
    legacy_dir = os.path.join(tmp_dir, "Legacy Videos")
    with open(cfg_path, "w", encoding="utf-8") as handle:
        json.dump({"video_dir": legacy_dir}, handle)

    config = Config(cfg_path)

    assert config.video_dir == os.path.abspath(legacy_dir)
    assert config.video_dirs == [os.path.abspath(legacy_dir)]


def test_config_set_video_dirs_updates_primary_video_dir(tmp_dir):
    cfg_path = os.path.join(tmp_dir, "config.json")
    config = Config(cfg_path)
    first = os.path.join(tmp_dir, "Videos One")
    second = os.path.join(tmp_dir, "Videos Two")

    config.video_dirs = [first, second]

    assert config.video_dir == os.path.abspath(first)
    assert config.video_dirs == [os.path.abspath(first), os.path.abspath(second)]
