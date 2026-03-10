"""
ui/console_tab.py
Raw serial log console — shows all TX/RX traffic. Useful for debugging.
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton,
    QTextEdit, QLineEdit, QLabel, QCheckBox
)
from PyQt5.QtCore import Qt, pyqtSlot
from PyQt5.QtGui import QTextCursor, QColor, QTextCharFormat, QFont


class ConsoleTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._teensy  = None
        self._paused  = False
        self._build_ui()

    def set_teensy(self, teensy):
        self._teensy = teensy

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(8, 8, 8, 8)
        root.setSpacing(6)

        # ── Toolbar ───────────────────────────────────────────────────────
        toolbar = QHBoxLayout()

        self.btn_clear  = QPushButton("Clear")
        self.btn_pause  = QPushButton("Pause")
        self.chk_scroll = QCheckBox("Auto-scroll")
        self.chk_scroll.setChecked(True)

        self.btn_clear.clicked.connect(self._clear)
        self.btn_pause.clicked.connect(self._toggle_pause)

        toolbar.addWidget(self.btn_clear)
        toolbar.addWidget(self.btn_pause)
        toolbar.addWidget(self.chk_scroll)
        toolbar.addStretch()
        toolbar.addWidget(QLabel("Max 2000 lines shown"))

        # ── Log area ──────────────────────────────────────────────────────
        self.log = QTextEdit()
        self.log.setReadOnly(True)
        self.log.setFont(QFont("Consolas", 11))
        self.log.setStyleSheet(
            "QTextEdit { background: #070a0d; color: #2dff6e; "
            "border: 1px solid #1a2332; }"
        )

        # ── Manual TX ────────────────────────────────────────────────────
        tx_row = QHBoxLayout()
        tx_row.addWidget(QLabel("Send:"))
        self.txt_cmd = QLineEdit()
        self.txt_cmd.setPlaceholderText("CMD:PING  or any raw command...")
        self.txt_cmd.returnPressed.connect(self._send_cmd)
        self.btn_send = QPushButton("Send")
        self.btn_send.clicked.connect(self._send_cmd)

        tx_row.addWidget(self.txt_cmd, 1)
        tx_row.addWidget(self.btn_send)

        root.addLayout(toolbar)
        root.addWidget(self.log, 1)
        root.addLayout(tx_row)

    # ── Public: called by main window whenever a log line arrives ─────────────

    def append_log(self, msg: str):
        if self._paused:
            return

        cursor = self.log.textCursor()
        cursor.movePosition(QTextCursor.End)

        fmt = QTextCharFormat()
        if msg.startswith("→"):
            fmt.setForeground(QColor("#00d4ff"))   # TX: cyan
        elif msg.startswith("←"):
            fmt.setForeground(QColor("#2dff6e"))   # RX: green
        elif msg.startswith("[AUTO"):
            fmt.setForeground(QColor("#ff9900"))   # system: orange
        elif "ERR" in msg or "error" in msg.lower():
            fmt.setForeground(QColor("#ff3333"))   # error: red
        else:
            fmt.setForeground(QColor("#bccdd8"))   # default

        cursor.insertText(msg + "\n", fmt)

        # Trim to 2000 lines
        doc = self.log.document()
        if doc.blockCount() > 2000:
            cursor2 = QTextCursor(doc.begin())
            cursor2.select(QTextCursor.BlockUnderCursor)
            cursor2.removeSelectedText()
            cursor2.deleteChar()

        if self.chk_scroll.isChecked():
            self.log.setTextCursor(cursor)
            self.log.ensureCursorVisible()

    # ── Actions ──────────────────────────────────────────────────────────────

    def _clear(self):
        self.log.clear()

    def _toggle_pause(self):
        self._paused = not self._paused
        self.btn_pause.setText("Resume" if self._paused else "Pause")

    def _send_cmd(self):
        cmd = self.txt_cmd.text().strip()
        if cmd and self._teensy:
            self._teensy.send_command(cmd)
            self.txt_cmd.clear()
