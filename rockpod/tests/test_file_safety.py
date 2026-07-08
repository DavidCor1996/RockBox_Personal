import os

from services.file_safety import atomic_write_json, atomic_write_text


def test_atomic_write_text_replaces_existing_file_without_temp_leftover(tmp_dir):
    path = os.path.join(tmp_dir, "nested", "state.tsv")
    atomic_write_text(path, "old\n")

    written = atomic_write_text(path, "new\n")

    assert written == os.path.abspath(path)
    with open(path, "r", encoding="utf-8") as handle:
        assert handle.read() == "new\n"
    assert os.listdir(os.path.dirname(path)) == ["state.tsv"]


def test_atomic_write_json_replaces_existing_file_without_temp_leftover(tmp_dir):
    path = os.path.join(tmp_dir, "state", "manifest.json")
    atomic_write_json(path, {"old": True})

    written = atomic_write_json(path, {"new": ["value"]})

    assert written == os.path.abspath(path)
    with open(path, "r", encoding="utf-8") as handle:
        assert handle.read() == '{\n  "new": [\n    "value"\n  ]\n}\n'
    assert os.listdir(os.path.dirname(path)) == ["manifest.json"]
