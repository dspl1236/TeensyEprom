"""
ui/gauges_tab.py
Live data gauges — AFR, MAP, RPM, TPS, IAT, Fuel Trim, Knock
"""

import time
from collections import deque
from PyQt5.QtWidgets import (
    QWidget, QGridLayout, QVBoxLayout, QHBoxLayout,
    QLabel, QGroupBox, QFrame, QSizePolicy, QSlider, QDoubleSpinBox
)
from PyQt5.QtCore import Qt, QTimer, pyqtSlot
from PyQt5.QtGui import QPainter, QColor, QFont, QPen, QBrush, QLinearGradient

from serial_comm.protocol import LiveData


# ── Single Gauge Widget ──────────────────────────────────────────────────────

class GaugeWidget(QFrame):
    """
    Numeric gauge with label, value, unit, and colour-coded background strip.
    """
    def __init__(self, title: str, unit: str, lo: float, hi: float,
                 warn_lo=None, warn_hi=None, crit_hi=None,
                 fmt="{:.1f}", parent=None):
        super().__init__(parent)
        self.title   = title
        self.unit    = unit
        self.lo      = lo
        self.hi      = hi
        self.warn_lo = warn_lo
        self.warn_hi = warn_hi
        self.crit_hi = crit_hi
        self.fmt     = fmt
        self._value  = 0.0
        self._hist   = deque(maxlen=120)  # 12s at 10Hz

        self.setFixedSize(170, 110)
        self.setFrameShape(QFrame.StyledPanel)
        self._build_ui()

    def _build_ui(self):
        lay = QVBoxLayout(self)
        lay.setContentsMargins(10, 8, 10, 8)
        lay.setSpacing(2)

        self.lbl_title = QLabel(self.title.upper())
        self.lbl_title.setAlignment(Qt.AlignCenter)
        self.lbl_title.setStyleSheet(
            "color: #3d5068; font-size: 10px; letter-spacing: 2px;"
        )

        self.lbl_value = QLabel("—")
        self.lbl_value.setAlignment(Qt.AlignCenter)
        self.lbl_value.setStyleSheet(
            "color: #00d4ff; font-size: 30px; font-family: 'Consolas', monospace; font-weight: bold;"
        )

        self.lbl_unit = QLabel(self.unit)
        self.lbl_unit.setAlignment(Qt.AlignCenter)
        self.lbl_unit.setStyleSheet("color: #3d5068; font-size: 11px;")

        lay.addWidget(self.lbl_title)
        lay.addWidget(self.lbl_value)
        lay.addWidget(self.lbl_unit)

    def update_value(self, v: float):
        self._value = v
        self._hist.append(v)
        self.lbl_value.setText(self.fmt.format(v))

        # Colour coding
        color = "#00d4ff"  # default cyan
        if self.crit_hi and v >= self.crit_hi:
            color = "#ff3333"
            self.setStyleSheet("QFrame { border: 1px solid #ff3333; background: #1a0808; }")
        elif self.warn_hi and v >= self.warn_hi:
            color = "#ff9900"
            self.setStyleSheet("QFrame { border: 1px solid #ff9900; background: #141008; }")
        elif self.warn_lo and v <= self.warn_lo:
            color = "#ff9900"
            self.setStyleSheet("QFrame { border: 1px solid #ff9900; background: #141008; }")
        else:
            color = "#2dff6e"
            self.setStyleSheet("QFrame { border: 1px solid #1a2332; background: #080b0e; }")

        self.lbl_value.setStyleSheet(
            f"color: {color}; font-size: 30px; font-family: 'Consolas', monospace; font-weight: bold;"
        )


class AFRGauge(GaugeWidget):
    """AFR gauge with rich target bar."""
    def __init__(self, parent=None):
        super().__init__("AFR", "λ", 10.0, 20.0,
                         warn_lo=12.0, warn_hi=15.8, crit_hi=17.0,
                         fmt="{:.2f}", parent=parent)
        self._target = 14.7

    def set_target(self, t: float):
        self._target = t

    def update_value(self, v: float):
        self._value = v
        self._hist.append(v)
        self.lbl_value.setText(self.fmt.format(v))

        # Rich colour: rich=red, lean=orange, ideal=green
        if v < 12.5:
            color, bg, border = "#ff3333", "#1a0808", "#ff3333"
        elif v < 13.5:
            color, bg, border = "#ff9900", "#141008", "#ff9900"
        elif v <= 15.0:
            color, bg, border = "#2dff6e", "#081408", "#2dff6e"
        elif v <= 16.5:
            color, bg, border = "#ff9900", "#141008", "#ff9900"
        else:
            color, bg, border = "#ff3333", "#1a0808", "#ff3333"

        self.setStyleSheet(f"QFrame {{ border: 1px solid {border}; background: {bg}; }}")
        self.lbl_value.setStyleSheet(
            f"color: {color}; font-size: 30px; font-family: 'Consolas', monospace; font-weight: bold;"
        )


# ── Mini sparkline graph ─────────────────────────────────────────────────────

class Sparkline(QWidget):
    """Tiny scrolling line chart for a single value."""
    def __init__(self, lo: float, hi: float, color="#00d4ff", parent=None):
        super().__init__(parent)
        self.lo = lo
        self.hi = hi
        self.color = QColor(color)
        self._data: deque = deque(maxlen=120)
        self.setMinimumHeight(50)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)

    def push(self, v: float):
        self._data.append(v)
        self.update()

    def paintEvent(self, event):
        if len(self._data) < 2:
            return
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        w, h = self.width(), self.height()
        rng = self.hi - self.lo or 1

        pen = QPen(self.color, 1.5)
        p.setPen(pen)
        p.fillRect(0, 0, w, h, QColor("#0d1117"))

        pts = list(self._data)
        n = len(pts)
        for i in range(1, n):
            x0 = int((i - 1) / (n - 1) * w)
            x1 = int(i / (n - 1) * w)
            y0 = h - int((pts[i - 1] - self.lo) / rng * h)
            y1 = h - int((pts[i] - self.lo) / rng * h)
            y0 = max(1, min(h - 1, y0))
            y1 = max(1, min(h - 1, y1))
            p.drawLine(x0, y0, x1, y1)


# ── Gauges Tab ───────────────────────────────────────────────────────────────

class GaugesTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._teensy = None
        self._last_data: LiveData = LiveData()
        self._build_ui()

        # Decay timer — grey out if no data for 2s
        self._data_ts = 0.0
        self._decay_timer = QTimer(self)
        self._decay_timer.timeout.connect(self._check_decay)
        self._decay_timer.start(500)

    def set_teensy(self, teensy):
        self._teensy = teensy
        if teensy:
            teensy.on_live_data = self._on_data

    def _build_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(12, 12, 12, 12)
        root.setSpacing(12)

        # ── Top row: gauges ───────────────────────────────────────────────
        gauge_row = QHBoxLayout()
        gauge_row.setSpacing(10)

        self.g_afr   = AFRGauge()
        self.g_rpm   = GaugeWidget("RPM",  "rpm", 0, 8000,
                                   warn_hi=6500, crit_hi=7200, fmt="{:.0f}")
        self.g_map   = GaugeWidget("MAP",  "kPa", 10, 250,
                                   warn_hi=210, fmt="{:.1f}")
        self.g_tps   = GaugeWidget("TPS",  "%",   0, 100, fmt="{:.1f}")
        self.g_iat   = GaugeWidget("IAT",  "°C",  -20, 100,
                                   warn_hi=60, crit_hi=80, fmt="{:.1f}")
        self.g_trim  = GaugeWidget("FUEL TRIM", "%", -15, 15,
                                   warn_lo=-8, warn_hi=8,
                                   crit_hi=13, fmt="{:+.1f}")
        self.g_knock = GaugeWidget("KNOCK", "°ret", 0, 10,
                                   warn_hi=3, crit_hi=7, fmt="{:.1f}")

        for g in [self.g_afr, self.g_rpm, self.g_map, self.g_tps,
                  self.g_iat, self.g_trim, self.g_knock]:
            gauge_row.addWidget(g)

        gauge_row.addStretch()

        # ── Target AFR control ────────────────────────────────────────────
        afr_row = QHBoxLayout()
        afr_row.setSpacing(8)
        afr_row.addWidget(QLabel("Target AFR:"))
        self.spin_target_afr = QDoubleSpinBox()
        self.spin_target_afr.setRange(11.0, 17.0)
        self.spin_target_afr.setSingleStep(0.1)
        self.spin_target_afr.setValue(14.7)
        self.spin_target_afr.setFixedWidth(80)
        self.spin_target_afr.valueChanged.connect(self._on_target_afr_changed)
        afr_row.addWidget(self.spin_target_afr)
        afr_row.addStretch()

        # ── Sparklines ────────────────────────────────────────────────────
        grp_spark = QGroupBox("History  (12s)")
        spark_lay = QGridLayout(grp_spark)
        spark_lay.setSpacing(4)

        self.spark_afr  = Sparkline(10, 20,  "#2dff6e")
        self.spark_rpm  = Sparkline(0, 7500, "#00d4ff")
        self.spark_map  = Sparkline(10, 250, "#ff9900")
        self.spark_trim = Sparkline(-15, 15, "#ff6e6e")

        for col, (label, spark) in enumerate([
            ("AFR",       self.spark_afr),
            ("RPM ÷ 100", self.spark_rpm),
            ("MAP kPa",   self.spark_map),
            ("Fuel Trim %", self.spark_trim),
        ]):
            spark_lay.addWidget(QLabel(label), 0, col)
            spark_lay.addWidget(spark, 1, col)

        # ── Assemble ──────────────────────────────────────────────────────
        root.addLayout(gauge_row)
        root.addLayout(afr_row)
        root.addWidget(grp_spark)
        root.addStretch()

    # ── Callbacks ────────────────────────────────────────────────────────────

    def _on_data(self, data: LiveData):
        self._last_data = data
        self._data_ts = time.time()

        self.g_afr.update_value(data.afr)
        self.g_rpm.update_value(data.rpm)
        self.g_map.update_value(data.map_kpa)
        self.g_tps.update_value(data.tps_pct)
        self.g_iat.update_value(data.iat_c)
        self.g_trim.update_value(data.fuel_trim_pct)
        self.g_knock.update_value(data.knock_retard)

        self.spark_afr.push(data.afr)
        self.spark_rpm.push(data.rpm / 100.0)
        self.spark_map.push(data.map_kpa)
        self.spark_trim.push(data.fuel_trim_pct)

    def _check_decay(self):
        if self._data_ts and time.time() - self._data_ts > 2.0:
            for g in [self.g_afr, self.g_rpm, self.g_map, self.g_tps,
                      self.g_iat, self.g_trim, self.g_knock]:
                g.setStyleSheet("QFrame { border: 1px solid #1a2332; background: #080b0e; }")

    def _on_target_afr_changed(self, val: float):
        self.g_afr.set_target(val)
        if self._teensy:
            self._teensy.set_target_afr(val)
