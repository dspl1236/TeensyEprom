"""
ui/map_editor_tab.py
16×16 Fuel Map and Timing Map editors with heat-map colouring and live cell push.
"""

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTabWidget,
    QPushButton, QLabel, QGroupBox, QTableWidget, QTableWidgetItem,
    QHeaderView, QSizePolicy, QMessageBox, QSpinBox, QAbstractItemView
)
from PyQt5.QtCore import Qt, pyqtSlot
from PyQt5.QtGui import QColor, QFont, QBrush

from ecu_profiles import (
    RPM_AXIS_266D, LOAD_AXIS_266D,
    raw_to_display, display_to_raw,
)


# Axis labels for 893906266D (7A Late) maps — confirmed from 034 RIP Chip .ecu file
# Source: 7A_Late_Generic_1.01.ecu Java serialisation (yAxisFactor=25, xAxisFactor=0.3922)
# Axis values are HARDCODED in the ECU definition — they are NOT stored in the ROM.
#
# ORIENTATION: rows = RPM (Y axis), cols = Load kPa (X axis)
# This matches the 034 tool layout exactly.
#
# RPM axis: 16 breakpoints (Y, rows)
RPM_LABELS = [str(r) for r in RPM_AXIS_266D]   # 16 values

# Load axis: 16 breakpoints in kPa (X, cols)
LOAD_LABELS = [str(k) for k in LOAD_AXIS_266D]  # 16 values

ROWS = 16   # RPM rows    (16×16 confirmed from 7A_Late_Generic_1.01.ecu dimsLowRows=16)
COLS = 16   # Load kPa columns


def _interpolate_color(value: int, lo: int, hi: int) -> QColor:
    """Blue (lo) → Cyan → Green → Yellow → Red (hi)"""
    if hi == lo:
        return QColor("#1a2332")
    t = max(0.0, min(1.0, (value - lo) / (hi - lo)))
    if t < 0.25:
        # Blue → Cyan
        u = t / 0.25
        return QColor(int(0), int(u * 180), int(120 + u * 135))
    elif t < 0.5:
        # Cyan → Green
        u = (t - 0.25) / 0.25
        return QColor(0, int(180 + u * 75), int(255 - u * 255))
    elif t < 0.75:
        # Green → Yellow
        u = (t - 0.5) / 0.25
        return QColor(int(u * 255), 255, 0)
    else:
        # Yellow → Red
        u = (t - 0.75) / 0.25
        return QColor(255, int(255 - u * 255), 0)


class MapTable(QTableWidget):
    """
    16×16 editable map table with heat-map background colouring.
    Changes are pushed to Teensy immediately on edit commit.
    """
    def __init__(self, map_type: str, parent=None):
        super().__init__(ROWS, COLS, parent)
        self.map_type = map_type  # 'fuel' or 'timing'
        self._teensy  = None
        self._data    = [0] * (ROWS * COLS)
        self._lo      = 0
        self._hi      = 255

        self._setup_table()
        self.itemChanged.connect(self._on_cell_changed)

    def _setup_table(self):
        # Headers — cols = Load kPa, rows = RPM  (matches 034 RIP Chip tool)
        self.setHorizontalHeaderLabels(LOAD_LABELS)
        self.setVerticalHeaderLabels(RPM_LABELS)

        self.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.verticalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.horizontalHeader().setMinimumSectionSize(40)
        self.verticalHeader().setMinimumSectionSize(24)

        font = QFont("Consolas", 10)
        self.setFont(font)
        self.setEditTriggers(QAbstractItemView.DoubleClicked | QAbstractItemView.SelectedClicked)
        self.setSelectionMode(QAbstractItemView.SingleSelection)

        # Set default cells
        self.blockSignals(True)
        for r in range(ROWS):
            for c in range(COLS):
                item = QTableWidgetItem("0")
                item.setTextAlignment(Qt.AlignCenter)
                self.setItem(r, c, item)
        self.blockSignals(False)

    def load_data(self, data: list):
        """Load flat array (ROWS*COLS bytes) into table and apply colours."""
        if len(data) != ROWS * COLS:
            return
        self._data = list(data)
        self._lo = min(data)
        self._hi = max(data)

        self.blockSignals(True)
        for r in range(ROWS):
            for c in range(COLS):
                v = data[r * COLS + c]
                item = self.item(r, c) or QTableWidgetItem()
                item.setText(str(v))
                item.setTextAlignment(Qt.AlignCenter)

                bg = _interpolate_color(v, self._lo, self._hi)
                item.setBackground(QBrush(bg))

                # Text colour: white on dark, dark on bright
                brightness = bg.red() * 0.299 + bg.green() * 0.587 + bg.blue() * 0.114
                item.setForeground(QBrush(QColor("#080b0e") if brightness > 140 else QColor("#bccdd8")))

                self.setItem(r, c, item)
        self.blockSignals(False)

    def highlight_cell(self, row: int, col: int):
        """Highlight operating cell (called from live data)."""
        self.clearSelection()
        item = self.item(row, col)
        if item:
            item.setSelected(True)
            self.scrollToItem(item)

    def _on_cell_changed(self, item: QTableWidgetItem):
        try:
            v = int(item.text())
        except ValueError:
            item.setText(str(self._data[item.row() * COLS + item.column()]))
            return

        v = max(0, min(255, v))
        r, c = item.row(), item.column()
        self._data[r * COLS + c] = v
        item.setText(str(v))

        # Recolour cell
        bg = _interpolate_color(v, self._lo, self._hi)
        item.setBackground(QBrush(bg))
        brightness = bg.red() * 0.299 + bg.green() * 0.587 + bg.blue() * 0.114
        item.setForeground(QBrush(QColor("#080b0e") if brightness > 140 else QColor("#bccdd8")))

        # Push to Teensy if connected
        if self._teensy:
            self._teensy.set_cell(self.map_type, r, c, v)


class MapEditorTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._teensy = None
        self._build_ui()

    def set_teensy(self, teensy):
        self._teensy = teensy
        self.fuel_table._teensy   = teensy
        self.timing_table._teensy = teensy
        if teensy:
            teensy.on_fuel_map   = self._on_fuel_map
            teensy.on_timing_map = self._on_timing_map

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(10, 10, 10, 10)
        root.setSpacing(8)

        # ── Toolbar ───────────────────────────────────────────────────────
        toolbar = QHBoxLayout()

        self.btn_get_fuel   = QPushButton("↓  Pull Fuel Map")
        self.btn_get_timing = QPushButton("↓  Pull Timing Map")
        self.btn_push_all   = QPushButton("↑  Push All Changes")
        self.btn_save       = QPushButton("💾  Save to SD")
        self.lbl_info       = QLabel("Double-click a cell to edit. Changes push to Teensy RAM immediately.")
        self.lbl_info.setStyleSheet("color: #3d5068; font-size: 11px;")

        self.btn_get_fuel.clicked.connect(self._pull_fuel)
        self.btn_get_timing.clicked.connect(self._pull_timing)
        self.btn_save.clicked.connect(self._save_map)

        for b in [self.btn_get_fuel, self.btn_get_timing, self.btn_push_all, self.btn_save]:
            b.setEnabled(False)
            toolbar.addWidget(b)

        toolbar.addSpacing(12)
        toolbar.addWidget(self.lbl_info)
        toolbar.addStretch()

        # ── Map tabs ──────────────────────────────────────────────────────
        self.map_tabs = QTabWidget()

        # Fuel map
        fuel_widget = QWidget()
        fuel_lay = QVBoxLayout(fuel_widget)
        fuel_lay.setContentsMargins(0, 0, 0, 0)
        self.fuel_table = MapTable("fuel")
        fuel_lay.addWidget(QLabel(
            "Fuel Map — 16×16  |  Rows = RPM  |  Cols = Load (kPa)  "
            "|  Display = signed(native_byte) + 128  |  Stock range: 40–123",
            styleSheet="color:#3d5068; font-size:11px; padding: 4px 0;"
        ))
        fuel_lay.addWidget(self.fuel_table)
        self.map_tabs.addTab(fuel_widget, "Fuel Map")

        # Timing map
        timing_widget = QWidget()
        timing_lay = QVBoxLayout(timing_widget)
        timing_lay.setContentsMargins(0, 0, 0, 0)
        self.timing_table = MapTable("timing")
        timing_lay.addWidget(QLabel(
            "Timing Map — 16×16  |  Rows = RPM  |  Cols = Load (kPa)",
            styleSheet="color:#3d5068; font-size:11px; padding: 4px 0;"
        ))
        timing_lay.addWidget(self.timing_table)
        self.map_tabs.addTab(timing_widget, "Timing Map")

        root.addLayout(toolbar)
        root.addWidget(self.map_tabs)

    # ── Actions ──────────────────────────────────────────────────────────────

    def _pull_fuel(self):
        if self._teensy:
            self._teensy.request_fuel_map()

    def _pull_timing(self):
        if self._teensy:
            self._teensy.request_timing_map()

    def _save_map(self):
        if self._teensy:
            self._teensy.save_map()

    def _on_fuel_map(self, data: list):
        self.fuel_table.load_data(data)

    def _on_timing_map(self, data: list):
        self.timing_table.load_data(data)

    def on_connected(self):
        for b in [self.btn_get_fuel, self.btn_get_timing,
                  self.btn_push_all, self.btn_save]:
            b.setEnabled(True)
        # Auto-pull maps on connect
        self._pull_fuel()
        self._pull_timing()

    def on_disconnected(self):
        for b in [self.btn_get_fuel, self.btn_get_timing,
                  self.btn_push_all, self.btn_save]:
            b.setEnabled(False)

    def highlight_operating_cell(self, rpm: int, map_kpa: float):
        """Highlight current operating cell based on live RPM/MAP.
        Row = RPM, Col = Load kPa  (matches 034 RIP Chip tool orientation)
        """
        # RPM axis (rows) — 16 breakpoints from 7A_Late_Generic_1.01.ecu
        row = 0
        for i, r in enumerate(RPM_AXIS_266D):
            if rpm >= r:
                row = i

        # Load axis (cols) — 16 breakpoints in kPa
        col = 0
        for i, l in enumerate(LOAD_AXIS_266D):
            if map_kpa >= l:
                col = i

        self.fuel_table.highlight_cell(row, col)
        self.timing_table.highlight_cell(row, col)
