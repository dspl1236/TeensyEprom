"""
ui/connection_panel.py
Top connection bar — COM port selector, auto-detect, connect/disconnect, status
"""

from PyQt5.QtWidgets import (
    QWidget, QHBoxLayout, QVBoxLayout, QLabel, QComboBox,
    QPushButton, QGroupBox, QSizePolicy
)
from PyQt5.QtCore import Qt, QTimer, pyqtSignal
from PyQt5.QtGui import QColor
from serial_comm.protocol import TeensySerial, list_ports, find_teensy_port


class StatusLight(QWidget):
    """Simple coloured circle indicator."""
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(16, 16)
        self._color = QColor("#3d5068")

    def set_color(self, color: str):
        self._color = QColor(color)
        self.update()

    def paintEvent(self, event):
        from PyQt5.QtGui import QPainter, QBrush
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        p.setBrush(QBrush(self._color))
        p.setPen(Qt.NoPen)
        p.drawEllipse(2, 2, 12, 12)


class ConnectionPanel(QWidget):
    """
    Top bar: port selector, auto-detect, baud, connect/disconnect, status.
    Emits connected/disconnected signals for the main window.
    """
    sig_connected    = pyqtSignal(object)   # TeensySerial instance
    sig_disconnected = pyqtSignal()
    sig_log          = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.teensy: TeensySerial = None
        self._build_ui()
        self._refresh_ports()

        # Auto-refresh port list every 3s
        self._port_timer = QTimer(self)
        self._port_timer.timeout.connect(self._refresh_ports)
        self._port_timer.start(3000)

    def _build_ui(self):
        root = QHBoxLayout(self)
        root.setContentsMargins(8, 6, 8, 6)
        root.setSpacing(12)

        # ── Port selection ──────────────────────────────────────────────
        grp_port = QGroupBox("Serial Port")
        lay_port = QHBoxLayout(grp_port)
        lay_port.setContentsMargins(8, 14, 8, 8)
        lay_port.setSpacing(8)

        self.combo_port = QComboBox()
        self.combo_port.setMinimumWidth(200)
        self.combo_port.setToolTip("Select COM port")

        self.btn_refresh = QPushButton("⟳")
        self.btn_refresh.setFixedWidth(36)
        self.btn_refresh.setToolTip("Refresh port list")
        self.btn_refresh.clicked.connect(self._refresh_ports)

        self.btn_autodetect = QPushButton("Auto-detect Teensy")
        self.btn_autodetect.setToolTip("Scan for Teensy 4.1 by USB VID")
        self.btn_autodetect.clicked.connect(self._auto_detect)

        lay_port.addWidget(self.combo_port)
        lay_port.addWidget(self.btn_refresh)
        lay_port.addWidget(self.btn_autodetect)

        # ── Connect controls ─────────────────────────────────────────────
        grp_conn = QGroupBox("Connection")
        lay_conn = QHBoxLayout(grp_conn)
        lay_conn.setContentsMargins(8, 14, 8, 8)
        lay_conn.setSpacing(8)

        self.btn_connect = QPushButton("Connect")
        self.btn_connect.setObjectName("btn_connect")
        self.btn_connect.setFixedWidth(110)
        self.btn_connect.clicked.connect(self._on_connect)

        self.btn_disconnect = QPushButton("Disconnect")
        self.btn_disconnect.setObjectName("btn_disconnect")
        self.btn_disconnect.setFixedWidth(110)
        self.btn_disconnect.setEnabled(False)
        self.btn_disconnect.clicked.connect(self._on_disconnect)

        lay_conn.addWidget(self.btn_connect)
        lay_conn.addWidget(self.btn_disconnect)

        # ── Status ────────────────────────────────────────────────────────
        grp_status = QGroupBox("Status")
        lay_status = QHBoxLayout(grp_status)
        lay_status.setContentsMargins(8, 14, 8, 8)
        lay_status.setSpacing(8)

        self.light = StatusLight()
        self.lbl_status = QLabel("Disconnected")
        self.lbl_status.setObjectName("label_status")
        self.lbl_rom = QLabel("")
        self.lbl_rom.setObjectName("label_status")

        lay_status.addWidget(self.light)
        lay_status.addWidget(self.lbl_status)
        lay_status.addSpacing(16)
        lay_status.addWidget(QLabel("ROM:"))
        lay_status.addWidget(self.lbl_rom)

        # ── Assemble ──────────────────────────────────────────────────────
        root.addWidget(grp_port, 2)
        root.addWidget(grp_conn, 0)
        root.addWidget(grp_status, 1)

    # ── Port management ──────────────────────────────────────────────────────

    def _refresh_ports(self):
        current = self.combo_port.currentText()
        self.combo_port.blockSignals(True)
        self.combo_port.clear()

        ports = list_ports()
        for p in ports:
            label = p["device"]
            if p["description"] and p["description"] != "n/a":
                label += f"  —  {p['description']}"
            if p["is_teensy"]:
                label += "  ★ Teensy"
            self.combo_port.addItem(label, userData=p["device"])

        # Restore previous selection if still present
        idx = self.combo_port.findData(current)
        if idx >= 0:
            self.combo_port.setCurrentIndex(idx)

        self.combo_port.blockSignals(False)

        if self.combo_port.count() == 0:
            self.combo_port.addItem("No ports found", userData="")

    def _auto_detect(self):
        self._refresh_ports()
        port = find_teensy_port()
        if port:
            idx = self.combo_port.findData(port)
            if idx >= 0:
                self.combo_port.setCurrentIndex(idx)
            self._set_status(f"Teensy found on {port}", "#2dff6e")
            self.sig_log.emit(f"[AUTO-DETECT] Teensy found on {port}")
        else:
            self._set_status("Teensy not found — check USB", "#ff9900")
            self.sig_log.emit("[AUTO-DETECT] No Teensy detected by VID")

    # ── Connect / Disconnect ─────────────────────────────────────────────────

    def _on_connect(self):
        port = self.combo_port.currentData()
        if not port:
            self._set_status("No port selected", "#ff3333")
            return

        self.teensy = TeensySerial()
        self.teensy.on_status = self._on_teensy_status
        self.teensy.on_log    = lambda msg: self.sig_log.emit(msg)

        self._set_status(f"Connecting to {port}...", "#ff9900")
        ok = self.teensy.connect(port)

        if ok:
            self.btn_connect.setEnabled(False)
            self.btn_disconnect.setEnabled(True)
            self.combo_port.setEnabled(False)
            self.btn_autodetect.setEnabled(False)
            self._set_status(f"Connected — {port}", "#2dff6e")
            self.light.set_color("#2dff6e")
            self.sig_connected.emit(self.teensy)
        else:
            self._set_status(f"Failed: {self.teensy.status.error}", "#ff3333")
            self.light.set_color("#ff3333")
            self.teensy = None

    def _on_disconnect(self):
        if self.teensy:
            self.teensy.disconnect()
            self.teensy = None
        self._set_disconnected_ui()
        self.sig_disconnected.emit()

    def _on_teensy_status(self, status):
        from PyQt5.QtCore import QMetaObject, Qt
        # Called from reader thread — marshal to UI thread
        if status.ready:
            self.lbl_rom.setText(status.rom_file or "—")
            self._set_status("Ready", "#2dff6e")
        elif not status.connected:
            self._set_disconnected_ui()
            self.sig_disconnected.emit()

    def _set_disconnected_ui(self):
        self.btn_connect.setEnabled(True)
        self.btn_disconnect.setEnabled(False)
        self.combo_port.setEnabled(True)
        self.btn_autodetect.setEnabled(True)
        self.light.set_color("#3d5068")
        self.lbl_status.setText("Disconnected")
        self.lbl_rom.setText("")

    def _set_status(self, msg: str, color: str = "#bccdd8"):
        self.lbl_status.setText(msg)
        self.lbl_status.setStyleSheet(f"color: {color};")
