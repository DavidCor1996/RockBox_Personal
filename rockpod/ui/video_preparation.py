"""Review source, streams and rendition before starting a movie conversion."""
from __future__ import annotations
import os
import shutil
from PySide6.QtCore import QThread, Signal, QUrl
from PySide6.QtGui import QDesktopServices
from PySide6.QtWidgets import (QDialog, QVBoxLayout, QFormLayout, QComboBox,
    QSpinBox, QLabel, QPushButton, QHBoxLayout, QTextBrowser, QFileDialog)
from services.video_pipeline import VideoPipeline, options, probe
from services.video_rvp import VideoRvpTranscoder


class PreparationWorker(QThread):
    ready = Signal(object)
    failed = Signal(str)
    status = Signal(str)

    def __init__(self, operation, parent):
        super().__init__(parent)
        self.operation = operation

    def run(self):
        try:
            self.ready.emit(self.operation())
        except Exception as exc:
            self.failed.emit(str(exc))


class VideoPreparationDialog(QDialog):
    def __init__(self, row, config, mount, target, initial=None, parent=None):
        super().__init__(parent)
        self.setWindowTitle('Prepare ' + str(row.get('title') or 'Video'))
        self.resize(680, 610)
        self.row, self.mount, self.target = row, mount, target
        self.worker = None
        self.settings = options(initial)
        backend = VideoRvpTranscoder(os.path.join(config.cache_dir, 'device_video_rvp'),
                                    ffmpeg_path=config.get('ffmpeg_binary', ''))
        self.pipeline = VideoPipeline(backend)
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel(str(row.get('title') or row['file_path'])))
        form = QFormLayout(); layout.addLayout(form)
        self.format = self.combo(form, 'Format', [('Automatic for This Device', 'auto'), ('H.264 MP4', 'h264'), ('MPEG MPG', 'mpeg')], self.settings['format'])
        self.quality = self.combo(form, 'Quality', [('Space Saver', 'space'), ('Balanced', 'balanced'), ('TV Quality', 'tv'), ('Custom size / bitrate', 'custom')], self.settings['quality'])
        self.size = QSpinBox(); self.size.setRange(0, 2047); self.size.setSuffix(' MiB'); self.size.setSpecialValueText('No size budget')
        self.size.setValue(self.settings['target_mib']); form.addRow('Target size', self.size)
        self.bitrate = QSpinBox(); self.bitrate.setRange(0, 4000); self.bitrate.setSuffix(' kbit/s'); self.bitrate.setSpecialValueText('Use quality setting')
        self.bitrate.setValue(self.settings['bitrate']); form.addRow('Video bitrate', self.bitrate)
        self.audio = self.combo(form, 'Audio', [('Source default track', 'default'), ('No audio', 'none')], 'default')
        self.subtitle = self.combo(form, 'Subtitles', [('Off', None)], None)
        self.external_subtitle = QPushButton('Choose external SRT…')
        form.addRow('', self.external_subtitle)
        self.external_subtitle.clicked.connect(self.choose_subtitle)
        self.subtitle_mode = self.combo(form, 'Subtitle treatment', [('Switchable text', 'soft'), ('Burn into video', 'burn')], 'soft')
        self.deinterlace = self.combo(form, 'Deinterlace', [('From source metadata', 'auto'), ('On', 'on'), ('Off', 'off')], 'auto')
        self.color = self.combo(form, 'Source color', [('From metadata / documented fallback', 'auto'), ('HD BT.709', 'bt709'), ('SD NTSC', 'smpte170m'), ('SD PAL', 'bt470bg')], 'auto')
        self.review = QTextBrowser(); layout.addWidget(self.review)
        self.status = QLabel('Inspecting source…'); self.status.setWordWrap(True); layout.addWidget(self.status)
        actions = QHBoxLayout(); layout.addLayout(actions)
        self.inspect_button = QPushButton('Review decision'); actions.addWidget(self.inspect_button)
        self.preview_button = QPushButton('20-second preview'); actions.addWidget(self.preview_button)
        self.proceed_button = QPushButton('Prepare for sync'); actions.addWidget(self.proceed_button)
        self.cancel_button = QPushButton('Cancel'); actions.addWidget(self.cancel_button)
        self.proceed_button.setEnabled(False)
        self.cancel_button.clicked.connect(self.reject)
        self.proceed_button.clicked.connect(self.accept)
        self.inspect_button.clicked.connect(self.inspect)
        self.preview_button.clicked.connect(self.preview)
        for control in (self.format, self.quality, self.audio, self.subtitle, self.subtitle_mode, self.deinterlace, self.color):
            control.currentIndexChanged.connect(self.invalidate)
        self.size.valueChanged.connect(self.invalidate); self.bitrate.valueChanged.connect(self.invalidate)
        self.start(lambda: probe(row['file_path'], self.pipeline.ffprobe), self.probed)

    @staticmethod
    def combo(form, label, choices, selected):
        widget = QComboBox()
        for title, value in choices: widget.addItem(title, value)
        index = widget.findData(selected)
        if index >= 0: widget.setCurrentIndex(index)
        form.addRow(label, widget)
        return widget

    def invalidate(self, *_):
        self.proceed_button.setEnabled(False)

    def current_options(self):
        subtitle = self.subtitle.currentData()
        external = subtitle if isinstance(subtitle, str) else ''
        return options({'format': self.format.currentData(), 'quality': self.quality.currentData(),
            'target_mib': self.size.value(), 'bitrate': self.bitrate.value(),
            'audio': self.audio.currentData(), 'subtitle': None if external else subtitle,
            'subtitle_file': external,
            'subtitle_mode': self.subtitle_mode.currentData(), 'deinterlace': self.deinterlace.currentData(),
            'color': self.color.currentData()})

    def choose_subtitle(self):
        path, _ = QFileDialog.getOpenFileName(self, 'Choose subtitles', '', 'SRT subtitles (*.srt)')
        if path:
            self.subtitle.addItem(os.path.basename(path), path)
            self.subtitle.setCurrentIndex(self.subtitle.count()-1)

    def start(self, operation, receive):
        if self.worker and self.worker.isRunning(): return
        for button in (self.inspect_button, self.preview_button, self.proceed_button): button.setEnabled(False)
        self.worker = PreparationWorker(operation, self)
        self.worker.status.connect(self.status.setText)
        self.pipeline.backend._progress = self.worker.status.emit
        self.worker.ready.connect(receive)
        self.worker.failed.connect(lambda text: self.status.setText(text))
        self.worker.finished.connect(lambda: self.inspect_button.setEnabled(True))
        self.worker.finished.connect(lambda: self.preview_button.setEnabled(True))
        # A running probe/encode owns its process and buffers. Avoid destroying
        # its QThread when the dialog closes.
        for control in (self.format,self.quality,self.audio,self.subtitle,self.subtitle_mode,
                        self.deinterlace,self.color,self.size,self.bitrate,self.external_subtitle):
            control.setEnabled(False)
            self.worker.finished.connect(lambda control=control: control.setEnabled(True))
        self.cancel_button.setEnabled(False)
        self.worker.finished.connect(lambda: self.cancel_button.setEnabled(True))
        self.worker.start()

    def reject(self):
        if self.worker and self.worker.isRunning():
            self.status.setText('Finishing the current inspection or preview before closing…')
            return
        super().reject()

    def probed(self, info):
        for widget, streams in ((self.audio, info['audio_tracks']), (self.subtitle, info['subtitle_tracks'])):
            for stream in streams:
                tags = stream.get('tags', {})
                widget.addItem(f"Track {stream['index']}: {tags.get('language', 'und')} · {stream['codec_name']} · {tags.get('title', '')}", stream['index'])
        v = info['video']
        self.review.setPlainText(f"Source: {v['codec_name']} · {v['width']}×{v['height']} · {v.get('avg_frame_rate', '?')} fps\nChoose the audio and subtitles above, then review the decision.")
        self.status.setText('The original file is preserved. No movie conversion has started.')

    def inspect(self):
        self.settings = self.current_options()
        self.status.setText('Checking the full source and installed player…')
        self.start(lambda: self.pipeline.plan(self.row['file_path'], self.settings, self.mount, self.target), self.reviewed)

    def reviewed(self, plan):
        mib = plan['estimated_bytes'] / 1048576
        free = shutil.disk_usage(self.mount).free / 1048576
        text = (f"{plan['operation'].title()}: {plan['reason']}\n\n"
                f"{plan['width']}×{plan['height']} · {plan['fps']} fps · {plan['format'].upper()}\n"
                f"Video {plan['video_kbps']} kbit/s · Audio {plan['audio_kbps']} kbit/s\n"
                f"Audio track: {plan['audio'].get('tags', {}).get('language', 'und') if plan['audio'] else 'none'} · Subtitles: {plan['subtitle'].get('tags', {}).get('language', 'und') if plan['subtitle'] else 'off'}\n"
                f"Decoder: {'hardware H.264' if plan['format'] == 'h264' else 'software MPEG'}\n"
                f"Estimated size: {mib:.0f} MiB (variable bitrate); device free: {free:.0f} MiB\n\n"
                + '\n'.join(plan['warnings']))
        self.review.setPlainText(text)
        self.status.setText('Review complete. Preparation starts only when you choose Prepare for sync.')
        self.proceed_button.setEnabled(plan['estimated_bytes'] < shutil.disk_usage(self.mount).free)

    def preview(self):
        self.settings = self.current_options()
        self.status.setText('Preparing a 20-second sample from one-third into the movie…')
        self.start(lambda: self.pipeline.prepare(self.row, self.settings, self.mount, self.target, preview=True), self.preview_ready)

    def preview_ready(self, result):
        row, _info = result
        QDesktopServices.openUrl(QUrl.fromLocalFile(row['sync_source_path']))
        self.status.setText('Preview opened. Review the decision before preparing the complete movie.')
