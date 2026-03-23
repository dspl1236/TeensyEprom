"""
ui/map_switcher_tab.py
──────────────────────
Tab shown in Teensy Lite mode.
Lets the user manage up to 8 .bin files on the Teensy SD card:
  • Upload new maps from PC
  • Rename slots
  • Set active map
  • See which map is currently loaded (live from Teensy STATUS response)
"""

import os
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox,
    QLabel, QPushButton, QListWidget, QListWidgetItem,
    QFileDialog, QMessageBox, QProgressBar, QSizePolicy,
)
from PyQt5.QtCore import Qt, QTimer, pyqtSignal
from PyQt5.QtGui import QFont, QColor


# ── Palette helpers ───────────────────────────────────────────────────────────
_DARK  = "#0d1117"
_PANEL = "#0a0e14"
_BORD  = "#1a2332"
_TEXT  = "#bccdd8"
_DIM   = "#3d5068"
_CYAN  = "#00d4ff"
_GREEN = "#2dff6e"
_RED   = "#ff3333"
_AMBER = "#ffa040"

_BTN_STYLE = (
    "QPushButton {"
    f"  background:{_PANEL}; color:{_TEXT}; border:1px solid {_BORD};"
    "  padding:6px 14px; font-size:12px; border-radius:2px; }"
    f"QPushButton:hover {{ background:#101820; border-color:{_CYAN}; color:{_CYAN}; }}"
    "QPushButton:disabled { color:#1e2d3d; border-color:#111820; }"
)


class MapSlotItem(QListWidgetItem):
    """One row in the map list."""
    def __init__(self, slot: int, filename: str = "", active: bool = False):
        label = f"  Map {slot}  —  {filename if filename else '(empty)'}"
        super().__init__(label)
        self.slot     = slot
        self.filename = filename
        self.active   = active
        self._refresh_style()

    def set_active(self, active: bool):
        self.active = active
        self._refresh_style()

    def set_filename(self, name: str):
        self.filename = name
        label = f"  Map {self.slot}  —  {name if name else '(empty)'}"
        if self.active:
            label = "▶ " + label.strip()
        self.setText(label)
        self._refresh_style()

    def _refresh_style(self):
        if self.active:
            self.setForeground(QColor(_GREEN))
            label = f"▶   Map {self.slot}  —  {self.filename if self.filename else '(empty)'}"
        else:
            self.setForeground(QColor(_TEXT if self.filename else _DIM))
            label = f"     Map {self.slot}  —  {self.filename if self.filename else '(empty)'}"
        self.setText(label)


class MapSwitcherTab(QWidget):
    sig_log = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._teensy     = None
        self._map_count  = 0
        self._active_map = 0   # 1-based, 0 = unknown
        self._poll_timer = QTimer(self)
        self._poll_timer.timeout.connect(self._poll_status)
        self._build_ui()

    # ── UI build ──────────────────────────────────────────────────────────────

    def _build_ui(self):
        self.setStyleSheet(f"background:{_DARK}; color:{_TEXT};")
        root = QVBoxLayout(self)
        root.setContentsMargins(16, 16, 16, 16)
        root.setSpacing(12)

        # ── Header ────────────────────────────────────────────────────────
        hdr = QLabel("Map Switcher  —  Teensy Lite Mode")
        hdr.setStyleSheet(f"color:{_CYAN}; font-size:14px; font-weight:bold;")
        root.addWidget(hdr)

        sub = QLabel(
            "Manage up to 8 .bin maps on the Teensy SD card.  "
            "Select a slot and upload a .bin, or click a slot to activate it live."
        )
        sub.setStyleSheet(f"color:{_DIM}; font-size:11px;")
        sub.setWordWrap(True)
        root.addWidget(sub)

        content = QHBoxLayout()
        content.setSpacing(12)

        # ── Map list ──────────────────────────────────────────────────────
        grp_list = QGroupBox("SD Card Maps")
        grp_list.setStyleSheet(
            f"QGroupBox {{ color:{_DIM}; border:1px solid {_BORD}; "
            f"margin-top:8px; padding-top:8px; background:{_PANEL}; }}"
            f"QGroupBox::title {{ subcontrol-origin:margin; padding:0 4px; }}"
        )
        list_lay = QVBoxLayout(grp_list)

        self.lst_maps = QListWidget()
        self.lst_maps.setStyleSheet(
            f"QListWidget {{ background:{_DARK}; border:1px solid {_BORD}; "
            f"color:{_TEXT}; font-family:'Courier New',monospace; font-size:12px; }}"
            f"QListWidget::item:selected {{ background:#0f1e2d; color:{_CYAN}; }}"
        )
        self.lst_maps.setFont(QFont("Courier New", 11))
        self.lst_maps.currentRowChanged.connect(self._on_slot_selected)
        list_lay.addWidget(self.lst_maps)

        # Populate 8 empty slots
        for i in range(1, 9):
            self.lst_maps.addItem(MapSlotItem(i))

        content.addWidget(grp_list, 3)

        # ── Slot controls ────────────────────────────────────────────────
        grp_ctrl = QGroupBox("Slot Actions")
        grp_ctrl.setStyleSheet(grp_list.styleSheet())
        grp_ctrl.setFixedWidth(220)
        ctrl_lay = QVBoxLayout(grp_ctrl)
        ctrl_lay.setSpacing(8)

        self.btn_upload = QPushButton("⬆  Upload .bin to Slot")
        self.btn_upload.setStyleSheet(_BTN_STYLE)
        self.btn_upload.setEnabled(False)
        self.btn_upload.setToolTip("Select a slot first, then upload a .bin file")
        self.btn_upload.clicked.connect(self._upload_map)
        ctrl_lay.addWidget(self.btn_upload)

        self.btn_activate = QPushButton("▶  Activate This Map")
        self.btn_activate.setStyleSheet(
            _BTN_STYLE.replace(_BORD, "#2dff6e").replace(_TEXT, _GREEN)
        )
        self.btn_activate.setEnabled(False)
        self.btn_activate.setToolTip("Switch Teensy to this map immediately")
        self.btn_activate.clicked.connect(self._activate_map)
        ctrl_lay.addWidget(self.btn_activate)

        ctrl_lay.addSpacing(8)

        self.btn_refresh = QPushButton("↺  Refresh List")
        self.btn_refresh.setStyleSheet(_BTN_STYLE)
        self.btn_refresh.clicked.connect(self._refresh_maps)
        ctrl_lay.addWidget(self.btn_refresh)

        ctrl_lay.addStretch()

        # Active map display
        self.lbl_active = QLabel("Active map: —")
        self.lbl_active.setStyleSheet(
            f"color:{_GREEN}; font-size:12px; font-family:monospace;"
        )
        self.lbl_active.setWordWrap(True)
        ctrl_lay.addWidget(self.lbl_active)

        self.lbl_sd_status = QLabel("SD: —")
        self.lbl_sd_status.setStyleSheet(f"color:{_DIM}; font-size:11px;")
        ctrl_lay.addWidget(self.lbl_sd_status)

        content.addWidget(grp_ctrl, 1)
        root.addLayout(content, 1)

        # ── Transfer progress ─────────────────────────────────────────────
        self.progress = QProgressBar()
        self.progress.setVisible(False)
        self.progress.setStyleSheet(
            f"QProgressBar {{ background:{_PANEL}; border:1px solid {_BORD}; "
            f"color:{_TEXT}; text-align:center; height:18px; }}"
            f"QProgressBar::chunk {{ background:{_CYAN}; }}"
        )
        root.addWidget(self.progress)

        # ── Log ───────────────────────────────────────────────────────────
        from PyQt5.QtWidgets import QTextEdit
        self.log_box = QTextEdit()
        self.log_box.setReadOnly(True)
        self.log_box.setMaximumHeight(100)
        self.log_box.setFont(QFont("Courier New", 10))
        self.log_box.setStyleSheet(
            f"QTextEdit {{ background:{_PANEL}; border:1px solid {_BORD}; "
            f"color:{_DIM}; font-size:10px; }}"
        )
        root.addWidget(self.log_box)

    # ── Teensy wiring ─────────────────────────────────────────────────────────

    def set_teensy(self, teensy):
        self._teensy = teensy
        if teensy:
            self._poll_timer.start(2000)
            self._refresh_maps()
            self._log("[MAP SWITCHER] Connected — Teensy Lite")
        else:
            self._poll_timer.stop()
            self._active_map = 0
            self.lbl_active.setText("Active map: —")
            self.lbl_sd_status.setText("SD: —")
            self._log("[MAP SWITCHER] Disconnected")

    # ── Slot list interactions ────────────────────────────────────────────────

    def _on_slot_selected(self, row: int):
        has_file = False
        if 0 <= row < self.lst_maps.count():
            item = self.lst_maps.item(row)
            has_file = bool(item.filename) if isinstance(item, MapSlotItem) else False
        self.btn_upload.setEnabled(True)
        self.btn_activate.setEnabled(bool(self._teensy) and has_file)

    def _get_selected_slot(self) -> int:
        """Return 1-based slot number of currently selected row, or 0."""
        row = self.lst_maps.currentRow()
        if row < 0:
            return 0
        item = self.lst_maps.item(row)
        return item.slot if isinstance(item, MapSlotItem) else row + 1

    # ── Upload ────────────────────────────────────────────────────────────────

    def _upload_map(self):
        slot = self._get_selected_slot()
        if not slot:
            return

        path, _ = QFileDialog.getOpenFileName(
            self, f"Select .bin for Map {slot}", "",
            "ROM Binary (*.bin);;All Files (*)"
        )
        if not path:
            return

        size = os.path.getsize(path)
        if size > 32768:
            QMessageBox.warning(self, "File Too Large",
                                f"ROM must be ≤ 32KB. File is {size} bytes.")
            return

        fname = os.path.basename(path)
        self._log(f"[UPLOAD] Map {slot} ← {fname}  ({size} bytes)")

        if not self._teensy:
            # Offline mode — just show what would happen
            self._mark_slot(slot, fname)
            self._log("[UPLOAD] Offline: slot updated in UI only. "
                      "Connect Teensy and use 'Send to SD' to transfer.")
            return

        # Send via USB: UPLOAD_MAP:slot:size\n then raw bytes
        try:
            with open(path, "rb") as f:
                data = f.read()

            self.progress.setVisible(True)
            self.progress.setRange(0, len(data))
            self.progress.setValue(0)

            cmd = f"UPLOAD_MAP:{slot}:{len(data)}\n"
            self._teensy.send_raw(cmd.encode())

            CHUNK = 256
            sent = 0
            while sent < len(data):
                chunk = data[sent:sent + CHUNK]
                self._teensy.send_raw(chunk)
                sent += len(chunk)
                self.progress.setValue(sent)

            self.progress.setVisible(False)
            self._mark_slot(slot, fname)
            self._log(f"[UPLOAD] Map {slot} sent OK")

        except Exception as e:
            self.progress.setVisible(False)
            QMessageBox.critical(self, "Upload Error", str(e))
            self._log(f"[UPLOAD] Error: {e}")

    def _mark_slot(self, slot: int, filename: str):
        item = self.lst_maps.item(slot - 1)
        if isinstance(item, MapSlotItem):
            item.set_filename(filename)

    # ── Activate ─────────────────────────────────────────────────────────────

    def _activate_map(self):
        slot = self._get_selected_slot()
        if not slot or not self._teensy:
            return
        cmd = f"LOAD_MAP:{slot}\n"
        self._teensy.send_raw(cmd.encode())
        self._log(f"[SWITCH] Requested map {slot}")
        QTimer.singleShot(500, self._poll_status)

    # ── Status poll ───────────────────────────────────────────────────────────

    def _refresh_maps(self):
        if not self._teensy:
            return
        self._teensy.send_raw(b"LIST_MAPS\n")
        QTimer.singleShot(300, self._poll_status)

    def _poll_status(self):
        if not self._teensy:
            return
        self._teensy.send_raw(b"STATUS\n")

    def on_teensy_line(self, line: str):
        """Called by main_window when Teensy sends a line in Lite mode."""
        line = line.strip()

        if line.startswith("STATUS:LITE:"):
            # STATUS:LITE:active_map:count:SD_OK|SD_FAIL
            parts = line.split(":")
            if len(parts) >= 5:
                try:
                    active = int(parts[2])
                    count  = int(parts[3])
                    sd_ok  = parts[4] == "SD_OK"
                    self._active_map = active
                    self._map_count  = count
                    self.lbl_active.setText(f"Active map: {active}  of  {count}")
                    self.lbl_sd_status.setText(f"SD: {'OK' if sd_ok else 'FAIL'}")
                    self.lbl_sd_status.setStyleSheet(
                        f"color:{'#2dff6e' if sd_ok else _RED}; font-size:11px;"
                    )
                    self._update_active_highlight(active)
                except ValueError:
                    pass

        elif line.startswith("MAP:"):
            # MAP:N:path[:ACTIVE]
            parts = line.split(":")
            if len(parts) >= 3:
                try:
                    slot = int(parts[1])
                    fname = os.path.basename(parts[2])
                    active = len(parts) >= 4 and parts[3] == "ACTIVE"
                    self._mark_slot(slot, fname)
                    if active:
                        self._update_active_highlight(slot)
                except (ValueError, IndexError):
                    pass

        elif line.startswith("MAP_LOADED:"):
            parts = line.split(":")
            if len(parts) >= 2:
                try:
                    slot = int(parts[1])
                    self._update_active_highlight(slot)
                    self._log(f"[SWITCH] Loaded map {slot}")
                except ValueError:
                    pass

    def _update_active_highlight(self, active_slot: int):
        for i in range(self.lst_maps.count()):
            item = self.lst_maps.item(i)
            if isinstance(item, MapSlotItem):
                item.set_active(item.slot == active_slot)

    # ── Log ───────────────────────────────────────────────────────────────────

    def _log(self, msg: str):
        self.log_box.append(msg)
        self.sig_log.emit(msg)
