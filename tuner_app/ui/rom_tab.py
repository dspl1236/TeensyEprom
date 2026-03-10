"""
ui/rom_tab.py
ROM file selector, corrections toggle, and SD card management.
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox,
    QPushButton, QLabel, QListWidget, QListWidgetItem,
    QCheckBox, QFrame, QSizePolicy
)
from PyQt5.QtCore import Qt


class RomTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._teensy = None
        self._build_ui()

    def set_teensy(self, teensy):
        self._teensy = teensy
        if teensy:
            teensy.on_rom_list = self._on_rom_list

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(12, 12, 12, 12)
        root.setSpacing(12)

        # ── ROM slot selector ─────────────────────────────────────────────
        grp_rom = QGroupBox("SD Card ROM Files")
        rom_lay = QVBoxLayout(grp_rom)

        rom_toolbar = QHBoxLayout()
        self.btn_list_roms = QPushButton("⟳  Refresh ROM List")
        self.btn_list_roms.clicked.connect(self._list_roms)
        self.btn_list_roms.setEnabled(False)

        self.btn_load_rom = QPushButton("▶  Load Selected ROM")
        self.btn_load_rom.clicked.connect(self._load_rom)
        self.btn_load_rom.setEnabled(False)

        rom_toolbar.addWidget(self.btn_list_roms)
        rom_toolbar.addWidget(self.btn_load_rom)
        rom_toolbar.addStretch()

        self.rom_list = QListWidget()
        self.rom_list.setMaximumHeight(160)
        self.rom_list.setStyleSheet(
            "QListWidget { background: #0d1117; border: 1px solid #1a2332; color: #bccdd8; }"
            "QListWidget::item:selected { background: #1a2840; color: #00d4ff; }"
        )

        self.lbl_current_rom = QLabel("Active ROM: —")
        self.lbl_current_rom.setStyleSheet("color: #2dff6e; font-size: 12px;")

        rom_lay.addLayout(rom_toolbar)
        rom_lay.addWidget(self.rom_list)
        rom_lay.addWidget(self.lbl_current_rom)

        # ── Corrections ───────────────────────────────────────────────────
        grp_corr = QGroupBox("Closed-Loop Corrections")
        corr_lay = QVBoxLayout(grp_corr)

        info = QLabel(
            "Corrections engine modifies romData[] in RAM based on wideband AFR feedback.\n"
            "Shadow copy prevents drift. Only affects current operating cell.\n"
            "Max fuel trim: ±10%   |   Max knock retard: 10°"
        )
        info.setStyleSheet("color: #3d5068; font-size: 11px;")
        info.setWordWrap(True)

        corr_btns = QHBoxLayout()
        self.btn_corr_on  = QPushButton("✔  Enable Corrections")
        self.btn_corr_off = QPushButton("✘  Disable Corrections")
        self.btn_corr_on.setEnabled(False)
        self.btn_corr_off.setEnabled(False)
        self.btn_corr_on.clicked.connect(self._corrections_on)
        self.btn_corr_off.clicked.connect(self._corrections_off)

        corr_btns.addWidget(self.btn_corr_on)
        corr_btns.addWidget(self.btn_corr_off)
        corr_btns.addStretch()

        self.lbl_corr_status = QLabel("Corrections: Unknown")
        self.lbl_corr_status.setStyleSheet("color: #3d5068;")

        corr_lay.addWidget(info)
        corr_lay.addLayout(corr_btns)
        corr_lay.addWidget(self.lbl_corr_status)

        root.addWidget(grp_rom)
        root.addWidget(grp_corr)
        root.addStretch()

    # ── Actions ──────────────────────────────────────────────────────────────

    def _list_roms(self):
        if self._teensy:
            self._teensy.list_roms()

    def _load_rom(self):
        item = self.rom_list.currentItem()
        if item and self._teensy:
            self._teensy.load_rom(item.text())

    def _corrections_on(self):
        if self._teensy:
            self._teensy.corrections_on()
            self.lbl_corr_status.setText("Corrections: ENABLED")
            self.lbl_corr_status.setStyleSheet("color: #2dff6e;")

    def _corrections_off(self):
        if self._teensy:
            self._teensy.corrections_off()
            self.lbl_corr_status.setText("Corrections: DISABLED")
            self.lbl_corr_status.setStyleSheet("color: #ff9900;")

    def _on_rom_list(self, roms: list):
        self.rom_list.clear()
        for r in roms:
            self.rom_list.addItem(r)

    def update_active_rom(self, rom_file: str):
        self.lbl_current_rom.setText(f"Active ROM: {rom_file}")

    def on_connected(self):
        for b in [self.btn_list_roms, self.btn_load_rom,
                  self.btn_corr_on, self.btn_corr_off]:
            b.setEnabled(True)
        self._list_roms()

    def on_disconnected(self):
        for b in [self.btn_list_roms, self.btn_load_rom,
                  self.btn_corr_on, self.btn_corr_off]:
            b.setEnabled(False)
        self.lbl_current_rom.setText("Active ROM: —")
        self.lbl_corr_status.setText("Corrections: Unknown")
        self.lbl_corr_status.setStyleSheet("color: #3d5068;")
