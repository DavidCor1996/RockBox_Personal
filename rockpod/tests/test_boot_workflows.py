from ui import boot_workflows


class _StatusBar:
    def __init__(self):
        self.text = []

    def set_left_text(self, value):
        self.text.append(value)


class _FakeProgress:
    created = []

    def __init__(self, label, cancel, minimum, maximum, parent):
        self.label = label
        self.cancel = cancel
        self.minimum = minimum
        self.maximum = maximum
        self.parent = parent
        self.title = ""
        self.modality = None
        self.minimum_duration = None
        self.auto_close = None
        self.auto_reset = None
        self.cancel_button = object()
        self.value = None
        self.closed = False
        self.deleted = False
        self.shown = False
        self.ranges = []
        _FakeProgress.created.append(self)

    def setWindowTitle(self, value):
        self.title = value

    def setWindowModality(self, value):
        self.modality = value

    def setMinimumDuration(self, value):
        self.minimum_duration = value

    def setAutoClose(self, value):
        self.auto_close = value

    def setAutoReset(self, value):
        self.auto_reset = value

    def setCancelButton(self, value):
        self.cancel_button = value

    def setValue(self, value):
        self.value = value

    def setRange(self, minimum, maximum):
        self.ranges.append((minimum, maximum))

    def setLabelText(self, value):
        self.label = value

    def show(self):
        self.shown = True

    def close(self):
        self.closed = True

    def deleteLater(self):
        self.deleted = True


def test_boot_progress_controller_show_update_and_close(monkeypatch):
    _FakeProgress.created = []
    process_events = []
    monkeypatch.setattr(boot_workflows, "QProgressDialog", _FakeProgress)
    monkeypatch.setattr(boot_workflows.QApplication, "processEvents", lambda: process_events.append(True))

    status = _StatusBar()
    controller = boot_workflows.BootProgressController(object(), status)

    progress = controller.show("Boot", "Starting", total=4)
    controller.update(progress, 2, 4, "Installing")
    controller.update(progress, 0, 0, "Building", busy=True)
    controller.close(progress)

    assert progress.title == "Boot"
    assert progress.label == "Building"
    assert progress.minimum == 0
    assert progress.maximum == 4
    assert progress.cancel_button is None
    assert progress.value == 2
    assert progress.ranges == [(0, 4), (0, 0)]
    assert progress.shown is True
    assert progress.closed is True
    assert progress.deleted is True
    assert status.text == ["Installing", "Building"]
    assert len(process_events) == 4
