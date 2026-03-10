"""
ui/datalog_tab.py
Load and plot CSV datalogs from the Teensy SD card.
Plots AFR, RPM, MAP, Fuel Trim, Knock over time.
"""

import csv
import os
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton,
    QLabel, QFileDialog, QCheckBox, QGroupBox, QSizePolicy
)
from PyQt5.QtCore import Qt

try:
    import matplotlib
    matplotlib.use("Qt5Agg")
    from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
    from matplotlib.figure import Figure
    import matplotlib.pyplot as plt
    HAS_MPL = True
except ImportError:
    HAS_MPL = False


class DatalogTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._data = {}
        self._build_ui()

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(12, 12, 12, 12)
        root.setSpacing(8)

        # ── Toolbar ───────────────────────────────────────────────────────
        toolbar = QHBoxLayout()
        self.btn_open = QPushButton("📂  Open Datalog CSV")
        self.btn_open.clicked.connect(self._open_csv)

        self.btn_clear = QPushButton("✘  Clear")
        self.btn_clear.clicked.connect(self._clear)
        self.btn_clear.setEnabled(False)

        self.lbl_file = QLabel("No file loaded")
        self.lbl_file.setStyleSheet("color: #3d5068; font-size: 11px;")

        toolbar.addWidget(self.btn_open)
        toolbar.addWidget(self.btn_clear)
        toolbar.addSpacing(12)
        toolbar.addWidget(self.lbl_file)
        toolbar.addStretch()

        # ── Channel toggles ───────────────────────────────────────────────
        grp_ch = QGroupBox("Channels")
        ch_lay = QHBoxLayout(grp_ch)
        grp_ch.setMaximumHeight(60)

        self._ch_boxes = {}
        channels = [
            ("AFR",        "#2dff6e"),
            ("RPM÷100",    "#00d4ff"),
            ("MAP_kPa",    "#ff9900"),
            ("FuelTrim%",  "#ff6e6e"),
            ("KnockRetard","#ff00ff"),
            ("IAT_C",      "#ffff00"),
        ]
        for name, color in channels:
            cb = QCheckBox(name)
            cb.setChecked(True)
            cb.setStyleSheet(f"QCheckBox {{ color: {color}; }}")
            cb.stateChanged.connect(self._replot)
            self._ch_boxes[name] = (cb, color)
            ch_lay.addWidget(cb)
        ch_lay.addStretch()

        # ── Plot area ─────────────────────────────────────────────────────
        if HAS_MPL:
            self.fig = Figure(facecolor="#0d1117", tight_layout=True)
            self.ax  = self.fig.add_subplot(111)
            self._style_axes()
            self.canvas = FigureCanvas(self.fig)
            self.canvas.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
            plot_widget = self.canvas
        else:
            plot_widget = QLabel(
                "matplotlib not available.\nInstall it with:  pip install matplotlib",
                alignment=Qt.AlignCenter,
            )
            plot_widget.setStyleSheet("color: #3d5068; font-size: 14px;")

        root.addLayout(toolbar)
        root.addWidget(grp_ch)
        root.addWidget(plot_widget, 1)

    def _style_axes(self):
        self.ax.set_facecolor("#0d1117")
        self.ax.tick_params(colors="#3d5068")
        self.ax.spines["bottom"].set_color("#1a2332")
        self.ax.spines["top"].set_color("#1a2332")
        self.ax.spines["left"].set_color("#1a2332")
        self.ax.spines["right"].set_color("#1a2332")
        self.ax.xaxis.label.set_color("#3d5068")
        self.ax.yaxis.label.set_color("#3d5068")
        self.ax.set_xlabel("Time (s)", color="#3d5068")
        self.ax.grid(True, color="#1a2332", linewidth=0.5)

    def _open_csv(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Open Datalog CSV", "", "CSV Files (*.csv);;All Files (*)"
        )
        if not path:
            return
        self._load_csv(path)

    def _load_csv(self, path: str):
        try:
            self._data = {}
            with open(path, newline="") as f:
                reader = csv.DictReader(f)
                cols = reader.fieldnames or []
                for col in cols:
                    self._data[col] = []
                for row in reader:
                    for col in cols:
                        try:
                            self._data[col].append(float(row[col]))
                        except (ValueError, KeyError):
                            self._data[col].append(0.0)

            self.lbl_file.setText(f"{os.path.basename(path)}  —  {len(self._data.get('time_s', []))} rows")
            self.btn_clear.setEnabled(True)
            self._replot()
        except Exception as e:
            self.lbl_file.setText(f"Error: {e}")

    def _replot(self):
        if not HAS_MPL or not self._data:
            return

        time_col = None
        for candidate in ["time_s", "time", "t", "timestamp"]:
            if candidate in self._data:
                time_col = self._data[candidate]
                break
        if time_col is None:
            time_col = list(range(len(next(iter(self._data.values())))))

        # Map CSV column names to display names and colours
        col_map = {
            "afr":         ("AFR",       "#2dff6e"),
            "rpm":         ("RPM÷100",   "#00d4ff"),
            "map_kpa":     ("MAP_kPa",   "#ff9900"),
            "fuel_trim":   ("FuelTrim%", "#ff6e6e"),
            "knock_retard":("KnockRetard","#ff00ff"),
            "iat_c":       ("IAT_C",     "#ffff00"),
        }

        self.ax.clear()
        self._style_axes()

        for csv_col, (disp_name, color) in col_map.items():
            cb, _ = self._ch_boxes.get(disp_name, (None, None))
            if cb and cb.isChecked() and csv_col in self._data:
                vals = self._data[csv_col]
                # RPM: divide by 100 for scale
                if csv_col == "rpm":
                    vals = [v / 100 for v in vals]
                self.ax.plot(time_col[:len(vals)], vals,
                             color=color, linewidth=1.2,
                             label=disp_name, alpha=0.9)

        self.ax.legend(facecolor="#0d1117", edgecolor="#1a2332",
                       labelcolor="#bccdd8", fontsize=9)
        self.canvas.draw()

    def _clear(self):
        self._data = {}
        self.lbl_file.setText("No file loaded")
        self.btn_clear.setEnabled(False)
        if HAS_MPL:
            self.ax.clear()
            self._style_axes()
            self.canvas.draw()
