import json
import re
import subprocess
from pathlib import Path
from types import SimpleNamespace

import pytest
from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication

from services.application_layout import APPLICATIONS, ORDER_PATH, ApplicationLayoutService
from ui.applications_panel import ApplicationsPanel

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture
def device(tmp_path):
    mount = tmp_path / 'ipod'
    (mount / '.rockbox/ipodjs').mkdir(parents=True)
    (mount / '.rockbox/rockbox-info.txt').write_text('Target: ipod6g\n')
    (mount / '.rockbox/config.cfg').write_text('tagcache_autoupdate: on\n')
    (mount / '.rockbox/database_idx.tcd').write_bytes(b'keep database')
    return mount


def test_save_round_trip_and_preserve_other_files(device):
    service = ApplicationLayoutService(ROOT)
    order = service.load(device)
    order.insert(0, order.pop(order.index('maps')))
    service.save(device, order)
    assert service.load(device) == order
    assert (device / '.rockbox/config.cfg').read_text() == 'tagcache_autoupdate: on\n'
    assert (device / '.rockbox/database_idx.tcd').read_bytes() == b'keep database'


def test_hidden_entries_keep_their_position(device):
    service = ApplicationLayoutService(ROOT)
    (device / '.rockbox/onlyfans').mkdir()
    (device / '.rockbox/onlyfans/hidden').touch()
    before = service.load(device)
    order = list(reversed(service.visible_ids(device)))
    service.save(device, order)
    result = service.load(device)
    assert result.index('onlyfans') == before.index('onlyfans')
    assert [key for key in result if key != 'onlyfans'] == order


def test_invalid_save_does_not_replace_existing_order(device):
    service = ApplicationLayoutService(ROOT)
    original = service.load(device)
    service.save(device, original)
    before = (device / ORDER_PATH).read_bytes()
    with pytest.raises(ValueError):
        service.save(device, original[:-1] + [original[0]])
    assert (device / ORDER_PATH).read_bytes() == before
    (device / '.rockbox/rockbox-info.txt').write_text('Target: ipodnano3g\n')
    with pytest.raises(ValueError):
        service.save(device, original)
    assert (device / ORDER_PATH).read_bytes() == before


def test_symbolic_order_path_rejected(device, tmp_path):
    service = ApplicationLayoutService(ROOT)
    outside = tmp_path / 'outside.txt'
    outside.write_text('unchanged')
    (device / ORDER_PATH).symlink_to(outside)
    with pytest.raises(ValueError):
        service.save(device, [key for key, _ in APPLICATIONS])
    assert outside.read_text() == 'unchanged'


def test_editor_moves_saves_and_stops_wiggle(device):
    app = QApplication.instance() or QApplication([])
    service = ApplicationLayoutService(ROOT)
    current = [SimpleNamespace(mount_path=str(device))]
    panel = ApplicationsPanel(service, lambda: current[0])
    panel.resize(720, 600)
    panel.show()
    app.processEvents()
    panel.arrange.setChecked(True)
    assert panel.grid.timer.isActive()
    first = panel.grid.ids()[0]
    panel.grid.move_item(0, 13)
    assert panel.grid.ids()[13] == first
    assert panel.dirty
    panel.refresh()
    assert panel.grid.ids()[13] == first  # draft survives refresh
    panel.save()
    assert service.load(device)[13] == first
    assert not panel.grid.timer.isActive()
    panel.arrange.setChecked(True)
    panel.hide()
    assert not panel.grid.timer.isActive()
    current[0] = None
    panel.refresh()
    assert panel.grid.count() == 0
    assert not panel.save_button.isEnabled()
    panel.close()


def test_actual_firmware_parser_matches_saved_order(device, tmp_path):
    source = (ROOT / 'apps/root_menu.c').read_text()
    table = source.split('root_menu_video_application_items[] = {', 1)[1].split('\n};', 1)[0]
    rows = re.findall(r'\{\s*"([^"]+)",\s*\w+,\s*"([^"]+)"\s*\}', table)
    assert [(icon.split('.')[0], label) for label, icon in rows] == list(APPLICATIONS)
    parser = source.split('#define IPODJS_APPLICATION_ORDER ', 1)[1].split('static bool root_menu_video_onlyfans_visible;', 1)[0]
    misc = (ROOT / 'apps/misc.c').read_text()
    read_line = 'int read_line(' + misc.split('int read_line(', 1)[1].split('\nchar* skip_whitespace', 1)[0]
    harness = '''#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define ARRAYLEN(a) (sizeof(a)/sizeof((a)[0]))
#define IPODJS_APPLICATION_ICON_CACHE 32
'''
    harness += '#define ROCKBOX_DIR ' + json.dumps(str(device / '.rockbox')) + '\n'
    harness += 'struct item { const char *icon; };\nstatic const struct item root_menu_video_application_items[] = {\n'
    harness += ',\n'.join('{' + json.dumps(icon) + '}' for _, icon in rows) + '\n};\n'
    harness += read_line + '\n#define IPODJS_APPLICATION_ORDER ' + parser
    harness += '\nint main(void) { root_menu_video_load_application_order(); for (unsigned i=0; i<ARRAYLEN(root_menu_video_application_items);i++) puts(root_menu_video_application_items[root_menu_video_application_order[i]].icon); return 0; }\n'
    path = tmp_path / 'parser.c'
    path.write_text(harness)
    binary = tmp_path / 'parser'
    subprocess.run(['cc', '-std=gnu99', '-Wall', '-Werror', str(path), '-o', str(binary)], check=True)
    service = ApplicationLayoutService(ROOT)
    cases = ['', 'maps\nyoutube\nmaps\nunknown\n', '\r\nclock\r\nweather\r\n',
             '\n'.join(reversed([key for key, _ in APPLICATIONS])) + '\n']
    for text in cases:
        (device / ORDER_PATH).write_text(text)
        actual = subprocess.check_output([str(binary)], text=True).splitlines()
        assert [icon.split('.')[0] for icon in actual] == service.load(device)


def test_all_catalog_icons_exist():
    service = ApplicationLayoutService(ROOT)
    assert all(service.icon_path(key).is_file() for key, _ in APPLICATIONS)


def test_wiggle_keyboard_and_drop_move_real_rows(device):
    from PySide6.QtCore import QPointF
    from PySide6.QtTest import QTest
    app = QApplication.instance() or QApplication([])
    panel = ApplicationsPanel(ApplicationLayoutService(ROOT),
                              lambda: SimpleNamespace(mount_path=str(device)))
    panel.show()
    app.processEvents()
    panel.arrange.setChecked(True)
    panel.grid.setCurrentRow(0)
    first = panel.grid.ids()[0]
    QTest.keyClick(panel.grid, Qt.Key_Right, Qt.ControlModifier)
    assert panel.grid.ids()[1] == first
    target = panel.grid.visualItemRect(panel.grid.item(5)).center()
    accepted = []
    event = SimpleNamespace(source=lambda: panel.grid,
                            position=lambda: QPointF(target),
                            setDropAction=lambda action: None,
                            accept=lambda: accepted.append(True),
                            ignore=lambda: accepted.append(False))
    panel.grid.dropEvent(event)
    assert accepted == [True]
    assert panel.grid.ids()[5] == first
    assert len(set(panel.grid.ids())) == len(APPLICATIONS)
    panel.close()
