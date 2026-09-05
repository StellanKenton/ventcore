#!/usr/bin/env python3

import argparse
import bisect
import csv
import math
import queue
import re
import socket
import subprocess
import tempfile
import threading
import time
import tkinter as tk
from datetime import datetime
from pathlib import Path
from tkinter import filedialog, font as tkfont, messagebox, ttk
from typing import Any, Callable, Dict, List, Optional

import device_tool


PEEP_VALUES = (5, 10, 15, 20, 25)
DELTA_PRESSURE_VALUES = (10, 15, 20, 25, 30)
TEST_DURATION_SECONDS = 10.0
STATUS_POLL_INTERVAL_SECONDS = 0.25
TOTAL_TEST_DURATION_SECONDS = (
    len(PEEP_VALUES) * len(DELTA_PRESSURE_VALUES) * TEST_DURATION_SECONDS
)
STATUS_TIMEOUT_SECONDS = 15.0
COMMAND_TIMEOUT_SECONDS = 5.0
CSV_PREFIX_FIELDS = [
    "test_index",
    "peep_cmh2o",
    "delta_pressure_cmh2o",
    "target_pressure_cmh2o",
    "sample_index",
    "test_elapsed_ms",
]
FIRMWARE_FIELD_NAMES = {
    "air_x2": "airFlowX2",
    "o2_x2": "oxygenFlowX2",
    "prox_x2": "proximalFlowX2",
    "pinsp_x1": "inspPressureX1",
    "ppeep_x1": "peepPressureX1",
    "pexp_x1": "expPressureX1",
    "ppat_x1": "patientPressureX1",
    "blower_x10": "blowerSpeedX10",
    "pref_x1": "patientRefPressureX1",
    "flowcomp_x1": "flowCompensationX1",
    "pcorr_x1": "patientCorrectionX1",
    "effort_x1": "innerEffortX1",
    "ff_x1": "blowerFeedforwardX1",
    "vt_x10": "tidalVolumeX10",
    "vti_x10": "tidalVolumeInspX10",
    "vte_x10": "tidalVolumeExpX10",
    "target_x100": "blowerTargetX100",
    "valve_x2": "valveDutyX2",
    "expiration_state": "expirationControllerState",
    "pressure_state": "pressureControllerState",
    "volume_pause": "volumePauseActive",
    "pause_settled": "volumePauseSettled",
    "leak_lpm": "leakFlowLpm",
}
FLOAT_FIRMWARE_FIELDS = set(FIRMWARE_FIELD_NAMES) - {
    "target_x100",
    "valve_x2",
    "expiration_state",
    "pressure_state",
    "volume_pause",
    "pause_settled",
}
EXPECTED_FIRMWARE_FIELDS = ["sequence", "time_ms", *FIRMWARE_FIELD_NAMES]
CSV_NON_WAVEFORM_FIELDS = set(CSV_PREFIX_FIELDS) | {"sequence", "time_ms"}
DEFAULT_PLOT_FIELDS = {
    "proximalFlowX2",
    "inspPressureX1",
    "patientPressureX1",
    "patientRefPressureX1",
}
PLOT_COLORS = (
    "#1565c0",
    "#c62828",
    "#2e7d32",
    "#6a1b9a",
    "#ef6c00",
    "#00838f",
    "#ad1457",
    "#4527a0",
    "#283593",
    "#0277bd",
    "#00897b",
    "#558b2f",
    "#9e9d24",
    "#f9a825",
    "#d84315",
    "#5d4037",
    "#546e7a",
    "#7b1fa2",
    "#00acc1",
    "#43a047",
)


class CollectionStopped(RuntimeError):
    pass


class RttSession:
    def __init__(self, profile: Dict[str, Any]) -> None:
        self.profile = profile
        self.server: Optional[subprocess.Popen[str]] = None
        self.sock: Optional[socket.socket] = None
        self.log_file: Optional[Any] = None
        self.log_path: Optional[Path] = None
        self.rx_buffer = ""

    def start(self) -> None:
        jlink = self.profile["jlink"]
        ports = jlink["ports"]
        gdb_server = device_tool.require_executable(
            str(jlink["gdb_server"]), "J-Link GDB server"
        )
        rtt_port = int(ports["rtt"])

        device_tool.kill_listeners_on_ports(int(port) for port in ports.values())
        device_tool.kill_processes_by_name(
            [
                "JLinkGDBServer",
                "JLinkGDBServerCL",
                "JLinkRTTClient",
                "JLinkRTTViewer",
                "JLinkGUIServer",
            ]
        )
        time.sleep(0.5)

        self.log_file = tempfile.NamedTemporaryFile(
            mode="w+", prefix="vent-test-rtt-", suffix=".log", delete=False
        )
        self.log_path = Path(self.log_file.name)
        command = [
            gdb_server,
            *device_tool.jlink_base_args(jlink),
            "-nohalt",
            "-port",
            str(ports["gdb"]),
            "-swoport",
            str(ports["swo"]),
            "-telnetport",
            str(ports["telnet"]),
            "-RTTTelnetPort",
            str(rtt_port),
        ]
        popen_args: Dict[str, Any] = {
            "stdout": self.log_file,
            "stderr": subprocess.STDOUT,
            "text": True,
        }
        if device_tool.normalized_os_name() == "windows":
            popen_args["creationflags"] = subprocess.CREATE_NO_WINDOW
        self.server = subprocess.Popen(command, **popen_args)
        self.sock = device_tool.connect_tcp_port(rtt_port, 15.0)
        if self.sock is None:
            server_output = self._server_output()
            self.close()
            raise device_tool.DeviceToolError(
                f"RTT port did not open: 127.0.0.1:{rtt_port}\n{server_output}"
            )
        self.sock.settimeout(0.2)

    def _server_output(self) -> str:
        if self.log_file is not None:
            self.log_file.flush()
        if self.log_path is None or not self.log_path.is_file():
            return ""
        return self.log_path.read_text(encoding="utf-8", errors="replace")

    def send_command(self, command: str) -> None:
        if self.sock is None:
            raise device_tool.DeviceToolError("RTT is not connected.")
        self.sock.sendall((command + "\n").encode("ascii"))

    def read_line(self, timeout_seconds: float, stop: threading.Event) -> str:
        if self.sock is None:
            raise device_tool.DeviceToolError("RTT is not connected.")
        deadline = time.monotonic() + timeout_seconds
        while time.monotonic() < deadline:
            if stop.is_set():
                raise CollectionStopped()
            newline_index = self.rx_buffer.find("\n")
            if newline_index >= 0:
                line = self.rx_buffer[:newline_index].rstrip("\r")
                self.rx_buffer = self.rx_buffer[newline_index + 1 :]
                return line
            if self.server is not None and self.server.poll() is not None:
                raise device_tool.DeviceToolError(
                    "J-Link GDB server exited unexpectedly.\n" + self._server_output()
                )
            try:
                data = self.sock.recv(4096)
            except socket.timeout:
                continue
            except OSError as exc:
                raise device_tool.DeviceToolError(f"RTT receive failed: {exc}") from exc
            if not data:
                raise device_tool.DeviceToolError("RTT connection was closed.")
            self.rx_buffer += data.decode("utf-8", errors="replace")
        raise TimeoutError("Timed out waiting for RTT output.")

    def close(self) -> None:
        if self.sock is not None:
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None
        if self.server is not None:
            self.server.terminate()
            try:
                self.server.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                self.server.kill()
                self.server.wait(timeout=3.0)
            self.server = None
        if self.log_file is not None:
            self.log_file.close()
            self.log_file = None
        if self.log_path is not None:
            try:
                self.log_path.unlink()
            except OSError:
                pass
            self.log_path = None


class VentTestCollector:
    def __init__(
        self,
        profile: Dict[str, Any],
        events: "queue.Queue[tuple[str, Any]]",
        stop: threading.Event,
    ) -> None:
        self.profile = profile
        self.events = events
        self.stop = stop
        self.session = RttSession(profile)
        self.rows: List[Dict[str, str]] = []
        self.waveform_fields: List[str] = []
        self.raw_waveform_fields: List[str] = []
        self.collection_first_time_ms: Optional[int] = None
        self.last_sequence: Optional[int] = None
        self.last_time_ms: Optional[int] = None
        self.group_first_time_ms: Dict[int, int] = {}
        self.group_sample_counts: Dict[int, int] = {}
        self.warnings: List[str] = []

    def emit(self, event: str, value: Any) -> None:
        self.events.put((event, value))

    def log(self, text: str) -> None:
        self.emit("log", text)

    def warn(self, text: str) -> None:
        self.warnings.append(text)
        self.log("警告：" + text)

    def wait_for_line(
        self, predicate: Callable[[str], bool], timeout_seconds: float
    ) -> str:
        deadline = time.monotonic() + timeout_seconds
        while time.monotonic() < deadline:
            try:
                line = self.session.read_line(
                    min(0.5, max(0.01, deadline - time.monotonic())), self.stop
                )
            except TimeoutError:
                continue
            if predicate(line):
                return line
        raise TimeoutError("Timed out waiting for expected RTT response.")

    def run_command(self, command: str, response_text: str) -> str:
        self.session.send_command(command)
        line = self.wait_for_line(
            lambda value: response_text in value, COMMAND_TIMEOUT_SECONDS
        )
        match = re.search(r"status=(-?\d+)", line)
        if match is not None and int(match.group(1)) != 1:
            raise device_tool.DeviceToolError(
                f"Command '{command}' failed: {line}"
            )
        return line

    def wait_console_ready(self) -> None:
        deadline = time.monotonic() + 15.0
        while time.monotonic() < deadline:
            self.session.send_command("help")
            try:
                self.wait_for_line(
                    lambda value: "[console] registered commands:" in value,
                    2.0,
                )
                return
            except TimeoutError:
                continue
        raise device_tool.DeviceToolError(
            "RTT connected, but the target console did not become ready."
        )

    def discard_status(self) -> None:
        self.session.send_command("vt status")
        self.wait_for_line(
            lambda value: value.startswith("VT_TRANSIENT_END"), STATUS_TIMEOUT_SECONDS
        )

    def capture_status(
        self, test_index: int, peep: int, delta_pressure: int
    ) -> int:
        self.session.send_command("vt status")
        deadline = time.monotonic() + STATUS_TIMEOUT_SECONDS
        header: Optional[List[str]] = None
        samples: List[List[str]] = []
        expected_count: Optional[int] = None
        end_count: Optional[int] = None
        first_sequence: Optional[int] = None
        dropped_count = 0
        sample_interval_ms: Optional[int] = None
        scale_received = False

        while time.monotonic() < deadline:
            try:
                line = self.session.read_line(
                    min(0.5, max(0.01, deadline - time.monotonic())), self.stop
                )
            except TimeoutError:
                continue
            if line.startswith("VT_TRANSIENT_BEGIN"):
                match = re.search(r"count=(\d+)", line)
                if match is not None:
                    expected_count = int(match.group(1))
                match = re.search(r"interval_ms=(\d+)", line)
                if match is not None:
                    sample_interval_ms = int(match.group(1))
                match = re.search(r"first_sequence=(\d+)", line)
                if match is not None:
                    first_sequence = int(match.group(1))
                match = re.search(r"dropped=(\d+)", line)
                if match is not None:
                    dropped_count = int(match.group(1))
                continue
            if line == "VT_MONITOR_SCALE,float_fields=100":
                scale_received = True
                continue
            if line.startswith("sequence,time_ms,"):
                header = next(csv.reader([line]))
                if header != EXPECTED_FIRMWARE_FIELDS:
                    raise device_tool.DeviceToolError(
                        "The target firmware does not provide the complete "
                        "gMonitorWaveformData CSV. Build and flash the current firmware first."
                    )
                continue
            if line.startswith("VT_TRANSIENT_END"):
                match = re.search(r"count=(\d+)", line)
                if match is not None:
                    end_count = int(match.group(1))
                break
            if header is not None and re.match(r"^\d+,\d+,", line):
                values = next(csv.reader([line]))
                if len(values) != len(header):
                    raise device_tool.DeviceToolError(
                        f"Incomplete CSV row in test {test_index}: {line}"
                    )
                samples.append(values)
        else:
            raise TimeoutError(f"Test {test_index}: VT_TRANSIENT_END not received.")

        if expected_count is None:
            raise device_tool.DeviceToolError(
                f"Test {test_index}: VT_TRANSIENT_BEGIN or its count is missing."
            )
        if not scale_received:
            raise device_tool.DeviceToolError(
                "The target firmware waveform scale marker is missing. "
                "Build and flash the current firmware first."
            )
        if header is None:
            raise device_tool.DeviceToolError(
                f"Test {test_index}: waveform CSV header not received."
            )
        if dropped_count:
            self.warn(
                f"第 {test_index:02d} 组本次 vt status 前已有 "
                f"{dropped_count} 个点被固件环形缓冲区覆盖。"
            )
        if end_count != expected_count:
            self.warn(
                f"第 {test_index:02d} 组 BEGIN count={expected_count}，"
                f"END count={end_count}。"
            )
        if len(samples) != expected_count:
            self.warn(
                f"第 {test_index:02d} 组本次声明 {expected_count} 行，"
                f"实际收到 {len(samples)} 行；继续保留已收到的数据。"
            )
        if self.raw_waveform_fields and header != self.raw_waveform_fields:
            raise device_tool.DeviceToolError("Waveform CSV fields changed during collection.")
        self.raw_waveform_fields = header
        self.waveform_fields = [FIRMWARE_FIELD_NAMES.get(field, field) for field in header]

        if samples and first_sequence is not None and int(samples[0][0]) != first_sequence:
            self.warn(
                f"第 {test_index:02d} 组响应首行序号 {samples[0][0]} 与 "
                f"first_sequence={first_sequence} 不一致。"
            )
        if sample_interval_ms is None:
            self.warn(f"第 {test_index:02d} 组响应缺少 interval_ms。")

        sequence_gap_count = 0
        missing_sequence_count = 0
        time_gap_count = 0
        first_gap: Optional[str] = None
        group_sample_index = self.group_sample_counts.get(test_index, 0)
        for values in samples:
            raw_waveform = dict(zip(header, values))
            sequence = int(raw_waveform["sequence"])
            device_time_ms = int(raw_waveform["time_ms"])
            if self.last_sequence is not None:
                expected_sequence = (self.last_sequence + 1) & 0xFFFFFFFF
                if sequence != expected_sequence:
                    sequence_gap_count += 1
                    sequence_delta = (sequence - expected_sequence) & 0xFFFFFFFF
                    if sequence_delta < 0x80000000:
                        missing_sequence_count += sequence_delta
                    if first_gap is None:
                        first_gap = f"{self.last_sequence} -> {sequence}"
            if self.last_time_ms is not None and sample_interval_ms is not None:
                expected_time_ms = (self.last_time_ms + sample_interval_ms) & 0xFFFFFFFF
                if device_time_ms != expected_time_ms:
                    time_gap_count += 1
            self.last_sequence = sequence
            self.last_time_ms = device_time_ms
            if self.collection_first_time_ms is None:
                self.collection_first_time_ms = device_time_ms
            if test_index not in self.group_first_time_ms:
                self.group_first_time_ms[test_index] = device_time_ms

            waveform = {
                FIRMWARE_FIELD_NAMES.get(field, field): (
                    f"{int(value) / 100.0:.2f}"
                    if field in FLOAT_FIRMWARE_FIELDS
                    else value
                )
                for field, value in raw_waveform.items()
            }
            combined = {
                "test_index": str(test_index),
                "peep_cmh2o": str(peep),
                "delta_pressure_cmh2o": str(delta_pressure),
                "target_pressure_cmh2o": str(peep + delta_pressure),
                "sample_index": str(group_sample_index),
                "test_elapsed_ms": str(
                    (device_time_ms - self.group_first_time_ms[test_index]) & 0xFFFFFFFF
                ),
            }
            combined.update(waveform)
            self.rows.append(combined)
            group_sample_index += 1
        self.group_sample_counts[test_index] = group_sample_index

        if sequence_gap_count:
            self.warn(
                f"第 {test_index:02d} 组本次发现 {sequence_gap_count} 处序号跳变，"
                f"估算缺少 {missing_sequence_count} 点，首处为 {first_gap}。"
            )
        if time_gap_count:
            self.warn(
                f"第 {test_index:02d} 组本次发现 {time_gap_count} 处时间戳不连续。"
            )
        return len(samples)

    def wait_until(
        self, deadline: float, collection_start: float, test_index: int
    ) -> None:
        while time.monotonic() < deadline:
            now = time.monotonic()
            if self.stop.wait(timeout=min(0.1, max(0.0, deadline - now))):
                raise CollectionStopped()
            elapsed = min(TOTAL_TEST_DURATION_SECONDS, time.monotonic() - collection_start)
            test_elapsed = min(
                TEST_DURATION_SECONDS,
                max(0.0, elapsed - (test_index - 1) * TEST_DURATION_SECONDS),
            )
            self.emit("countdown", (test_index, test_elapsed))

    @staticmethod
    def next_poll_deadline(deadline: float, now: float) -> float:
        while deadline <= now:
            deadline += STATUS_POLL_INTERVAL_SECONDS
        return deadline

    def run(self) -> None:
        tests = [
            (peep, delta_pressure)
            for peep in PEEP_VALUES
            for delta_pressure in DELTA_PRESSURE_VALUES
        ]
        outcome = "complete"
        try:
            self.log("正在启动 J-Link RTT 服务……")
            self.session.start()
            self.log("RTT 已连接，正在等待目标 console……")
            self.wait_console_ready()
            self.log("目标 console 已就绪，正在清理旧采样……")
            self.run_command("vt stop", "stop status=")
            peep, delta_pressure = tests[0]
            self.emit("test_started", (1, peep, delta_pressure))
            self.log(f"第 01/25 组：PEEP={peep}，ΔP={delta_pressure}，开始运行。")
            self.run_command(f"vt set {peep} {delta_pressure}", "set peep100=")
            self.run_command("vt pac", "PAC start status=")
            # Start the 250-second clock when the baseline status snapshot is sent.
            collection_start = time.monotonic()
            self.discard_status()

            collection_end = collection_start + TOTAL_TEST_DURATION_SECONDS
            current_test_index = 1
            next_scheme = collection_start + TEST_DURATION_SECONDS
            next_poll = collection_start + STATUS_POLL_INTERVAL_SECONDS
            self.log("连续采集已开始：每 250 ms 执行 vt status，总时长 250 s。")

            while True:
                target = min(next_poll, next_scheme, collection_end)
                self.wait_until(target, collection_start, current_test_index)
                now = time.monotonic()

                if now >= collection_end:
                    peep, delta_pressure = tests[current_test_index - 1]
                    self.capture_status(current_test_index, peep, delta_pressure)
                    sample_count = self.group_sample_counts.get(current_test_index, 0)
                    self.emit("test_finished", (current_test_index, sample_count))
                    self.log(
                        f"第 {current_test_index:02d}/25 组完成，累计收到 "
                        f"{sample_count} 个采样点。"
                    )
                    break

                if now >= next_scheme and current_test_index < len(tests):
                    peep, delta_pressure = tests[current_test_index - 1]
                    self.capture_status(current_test_index, peep, delta_pressure)
                    sample_count = self.group_sample_counts.get(current_test_index, 0)
                    self.emit("test_finished", (current_test_index, sample_count))
                    self.log(
                        f"第 {current_test_index:02d}/25 组完成，累计收到 "
                        f"{sample_count} 个采样点。"
                    )

                    current_test_index += 1
                    peep, delta_pressure = tests[current_test_index - 1]
                    self.emit(
                        "test_started", (current_test_index, peep, delta_pressure)
                    )
                    self.log(
                        f"第 {current_test_index:02d}/25 组：PEEP={peep}，"
                        f"ΔP={delta_pressure}，下发新方案。"
                    )
                    self.run_command(
                        f"vt set {peep} {delta_pressure}", "set peep100="
                    )
                    self.run_command("vt pac", "PAC start status=")
                    next_scheme = (
                        collection_start
                        + current_test_index * TEST_DURATION_SECONDS
                    )
                    next_poll = self.next_poll_deadline(next_poll, time.monotonic())
                    continue

                peep, delta_pressure = tests[current_test_index - 1]
                self.capture_status(current_test_index, peep, delta_pressure)
                next_poll = self.next_poll_deadline(next_poll, time.monotonic())

            self.run_command("vt stop", "stop status=")
            if self.rows:
                duration_ms = (
                    int(self.rows[-1]["time_ms"]) - int(self.rows[0]["time_ms"])
                ) & 0xFFFFFFFF
                self.log(
                    f"连续采集结束：共 {len(self.rows)} 点，固件时间跨度 "
                    f"{duration_ms / 1000.0:.3f} s，完整性警告 {len(self.warnings)} 条。"
                )
        except CollectionStopped:
            self.log("采集已由用户停止，已完成的数据可以导出。")
            outcome = "stopped"
        except Exception as exc:
            self.emit("error", str(exc))
            outcome = "stopped"
        finally:
            if self.session.sock is not None:
                try:
                    self.session.send_command("vt stop")
                except (OSError, device_tool.DeviceToolError):
                    pass
            self.session.close()
        self.emit(outcome, (self.rows, self.waveform_fields, self.warnings))


class WaveformViewer:
    def __init__(self, parent: tk.Misc, csv_path: Path) -> None:
        (
            self.rows_by_test,
            self.group_metadata,
            self.waveform_fields,
        ) = self._read_csv(csv_path)
        self.csv_path = csv_path
        self.window = tk.Toplevel(parent)
        self.window.title(f"Ventcore 波形查看 - {csv_path.name}")
        self.window.geometry("1180x760")
        self.window.minsize(820, 520)
        self.redraw_after_id: Optional[str] = None
        self.plot_panels: List[Dict[str, Any]] = []
        self.field_vars: Dict[str, tk.BooleanVar] = {}
        self.group_label_to_index: Dict[str, int] = {}
        self.info_var = tk.StringVar()

        style = ttk.Style(self.window)
        self.plot_background = style.lookup("TFrame", "background") or "white"
        self.plot_foreground = style.lookup("TLabel", "foreground") or "black"
        self.plot_grid = style.lookup("TSeparator", "background") or "#bdbdbd"
        self.plot_title_font = tkfont.nametofont("TkDefaultFont").copy()
        self.plot_title_font.configure(weight="bold")
        self._build_ui()
        self._group_changed()

    @staticmethod
    def _read_csv(
        csv_path: Path,
    ) -> tuple[
        Dict[int, List[Dict[str, float]]],
        Dict[int, tuple[float, float]],
        List[str],
    ]:
        try:
            handle = csv_path.open("r", encoding="utf-8-sig", newline="")
        except OSError as exc:
            raise ValueError(f"无法打开 CSV：{exc}") from exc

        with handle:
            reader = csv.DictReader(handle)
            fields = reader.fieldnames or []
            required = set(CSV_PREFIX_FIELDS) | {"sequence", "time_ms"}
            missing = sorted(required - set(fields))
            if missing:
                raise ValueError("CSV 缺少必要字段：" + ", ".join(missing))
            waveform_fields = [
                field for field in fields if field not in CSV_NON_WAVEFORM_FIELDS
            ]
            if not waveform_fields:
                raise ValueError("CSV 中没有可绘制的波形字段。")

            rows_by_test: Dict[int, List[Dict[str, float]]] = {}
            metadata: Dict[int, tuple[float, float]] = {}
            for row_number, row in enumerate(reader, start=2):
                try:
                    test_index = int(row["test_index"])
                    peep = float(row["peep_cmh2o"])
                    delta_pressure = float(row["delta_pressure_cmh2o"])
                    parsed = {
                        "test_elapsed_ms": float(row["test_elapsed_ms"]),
                        **{field: float(row[field]) for field in waveform_fields},
                    }
                except (KeyError, TypeError, ValueError) as exc:
                    raise ValueError(f"CSV 第 {row_number} 行包含无效数值。") from exc
                if not all(
                    math.isfinite(value)
                    for value in (peep, delta_pressure, *parsed.values())
                ):
                    raise ValueError(f"CSV 第 {row_number} 行包含非有限数值。")
                if test_index in metadata and metadata[test_index] != (
                    peep,
                    delta_pressure,
                ):
                    raise ValueError(f"CSV 第 {row_number} 行的测试组参数不一致。")
                rows_by_test.setdefault(test_index, []).append(parsed)
                metadata[test_index] = (peep, delta_pressure)

        if not rows_by_test:
            raise ValueError("CSV 中没有采样数据。")
        for rows in rows_by_test.values():
            rows.sort(key=lambda row: row["test_elapsed_ms"])
        return rows_by_test, metadata, waveform_fields

    @staticmethod
    def _number_text(value: float) -> str:
        absolute = abs(value)
        if absolute != 0.0 and (absolute >= 10000.0 or absolute < 0.01):
            return f"{value:.3g}"
        return f"{value:.2f}"

    def _build_ui(self) -> None:
        toolbar = ttk.Frame(self.window, padding=8)
        toolbar.pack(fill=tk.X)
        ttk.Label(toolbar, text="测试组：").pack(side=tk.LEFT)
        self.group_var = tk.StringVar()
        self.group_combo = ttk.Combobox(
            toolbar, textvariable=self.group_var, state="readonly", width=28
        )
        labels: List[str] = []
        for test_index in sorted(self.rows_by_test):
            peep, delta_pressure = self.group_metadata[test_index]
            label = (
                f"{test_index:02d} | PEEP={peep:g} | ΔP={delta_pressure:g}"
            )
            labels.append(label)
            self.group_label_to_index[label] = test_index
        self.group_combo["values"] = labels
        self.group_combo.current(0)
        self.group_combo.pack(side=tk.LEFT)
        self.group_combo.bind("<<ComboboxSelected>>", self._group_changed)
        ttk.Label(toolbar, textvariable=self.info_var).pack(side=tk.LEFT, padx=16)
        ttk.Label(toolbar, text=self.csv_path.name).pack(side=tk.RIGHT)

        body = ttk.Panedwindow(self.window, orient=tk.HORIZONTAL)
        body.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 8))

        controls = ttk.Frame(body, width=250)
        body.add(controls, weight=0)
        control_buttons = ttk.Frame(controls)
        control_buttons.pack(fill=tk.X, pady=(0, 6))
        ttk.Button(control_buttons, text="全选", command=self._select_all).pack(
            side=tk.LEFT
        )
        ttk.Button(control_buttons, text="清空", command=self._clear_all).pack(
            side=tk.LEFT, padx=6
        )

        variable_frame = ttk.Frame(controls)
        variable_frame.pack(fill=tk.BOTH, expand=True)
        self.variable_canvas = tk.Canvas(variable_frame, highlightthickness=0, width=230)
        variable_scroll = ttk.Scrollbar(
            variable_frame, orient=tk.VERTICAL, command=self.variable_canvas.yview
        )
        self.variable_canvas.configure(yscrollcommand=variable_scroll.set)
        self.variable_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        variable_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        variable_inner = ttk.Frame(self.variable_canvas)
        variable_window = self.variable_canvas.create_window(
            (0, 0), window=variable_inner, anchor=tk.NW
        )
        variable_inner.bind(
            "<Configure>",
            lambda _event: self.variable_canvas.configure(
                scrollregion=self.variable_canvas.bbox("all")
            ),
        )
        self.variable_canvas.bind(
            "<Configure>",
            lambda event: self.variable_canvas.itemconfigure(
                variable_window, width=event.width
            ),
        )
        for field in self.waveform_fields:
            selected = field in DEFAULT_PLOT_FIELDS
            variable = tk.BooleanVar(value=selected)
            self.field_vars[field] = variable
            ttk.Checkbutton(
                variable_inner,
                text=field,
                variable=variable,
                command=self._schedule_redraw,
            ).pack(anchor=tk.W, fill=tk.X, pady=1)
        if not any(variable.get() for variable in self.field_vars.values()):
            self.field_vars[self.waveform_fields[0]].set(True)

        plot_frame = ttk.Frame(body)
        body.add(plot_frame, weight=1)
        self.plot_canvas = tk.Canvas(
            plot_frame,
            background=self.plot_background,
            highlightthickness=0,
        )
        self.plot_canvas.pack(fill=tk.BOTH, expand=True)
        self.plot_canvas.bind("<Configure>", self._schedule_redraw)
        self.plot_canvas.bind("<Motion>", self._show_hover)
        self.plot_canvas.bind("<Leave>", self._clear_hover)

    def _selected_fields(self) -> List[str]:
        return [field for field in self.waveform_fields if self.field_vars[field].get()]

    def _select_all(self) -> None:
        for variable in self.field_vars.values():
            variable.set(True)
        self._schedule_redraw()

    def _clear_all(self) -> None:
        for variable in self.field_vars.values():
            variable.set(False)
        self._schedule_redraw()

    def _group_changed(self, _event: Optional[tk.Event] = None) -> None:
        test_index = self.group_label_to_index[self.group_var.get()]
        self._set_group_info(test_index)
        self.plot_canvas.yview_moveto(0.0)
        self._schedule_redraw()

    def _set_group_info(self, test_index: int) -> None:
        peep, delta_pressure = self.group_metadata[test_index]
        self.info_var.set(
            f"PEEP={peep:g}  ΔP={delta_pressure:g}  "
            f"采样点={len(self.rows_by_test[test_index])}"
        )

    def _schedule_redraw(self, _event: Optional[tk.Event] = None) -> None:
        if self.redraw_after_id is not None:
            self.window.after_cancel(self.redraw_after_id)
        self.redraw_after_id = self.window.after_idle(self._redraw)

    def _redraw(self) -> None:
        self.redraw_after_id = None
        canvas = self.plot_canvas
        canvas.delete("all")
        self.plot_panels = []
        selected_fields = self._selected_fields()
        width = max(canvas.winfo_width(), 480)
        viewport_height = max(canvas.winfo_height(), 260)
        if not selected_fields:
            canvas.configure(scrollregion=(0, 0, width, viewport_height))
            canvas.create_text(
                width / 2,
                viewport_height / 2,
                text="请勾选至少一个变量",
                fill=self.plot_foreground,
                font="TkDefaultFont",
            )
            return

        test_index = self.group_label_to_index[self.group_var.get()]
        rows = self.rows_by_test[test_index]
        x_values = [row["test_elapsed_ms"] / 1000.0 for row in rows]
        x_min = min(x_values)
        x_max = max(x_values)
        if x_max <= x_min:
            x_max = x_min + 1.0

        left = 62.0
        right = float(width - 20)
        legend_item_width = 185.0
        legend_columns = max(1, int((right - left) // legend_item_width))
        legend_rows = math.ceil(len(selected_fields) / legend_columns)
        plot_top = 18.0 + legend_rows * 22.0
        total_height = max(viewport_height, int(plot_top + 150.0))
        plot_bottom = float(total_height - 34)
        canvas.configure(scrollregion=(0, 0, width, total_height))
        canvas.create_rectangle(left, plot_top, right, plot_bottom, outline=self.plot_grid)
        for fraction, label in ((0.0, "0%"), (0.5, "50%"), (1.0, "100%")):
            y = plot_bottom - fraction * (plot_bottom - plot_top)
            canvas.create_line(left, y, right, y, fill=self.plot_grid)
            canvas.create_text(
                left - 6,
                y,
                text=label,
                anchor=tk.E,
                fill=self.plot_foreground,
                font="TkDefaultFont",
            )

        for series_index, field in enumerate(selected_fields):
            values = [row[field] for row in rows]
            y_min = min(values)
            y_max = max(values)
            if y_max <= y_min:
                padding = max(abs(y_min) * 0.05, 1.0)
            else:
                padding = (y_max - y_min) * 0.08
            y_low = y_min - padding
            y_high = y_max + padding
            color = PLOT_COLORS[self.waveform_fields.index(field) % len(PLOT_COLORS)]
            legend_column = series_index % legend_columns
            legend_row = series_index // legend_columns
            legend_x = left + legend_column * legend_item_width
            legend_y = 14.0 + legend_row * 22.0
            canvas.create_line(
                legend_x,
                legend_y,
                legend_x + 18,
                legend_y,
                fill=color,
                width=3,
            )
            canvas.create_text(
                legend_x + 24,
                legend_y,
                text=field,
                anchor=tk.W,
                fill=self.plot_foreground,
                font="TkDefaultFont",
            )

            points: List[float] = []
            for x_value, value in zip(x_values, values):
                x = left + (x_value - x_min) * (right - left) / (x_max - x_min)
                y = plot_bottom - (value - y_low) * (plot_bottom - plot_top) / (
                    y_high - y_low
                )
                points.extend((x, y))
            if len(points) >= 4:
                canvas.create_line(
                    *points,
                    fill=color,
                    width=2,
                )
            self.plot_panels.append(
                {
                    "field": field,
                    "x_values": x_values,
                    "values": values,
                    "x_min": x_min,
                    "x_max": x_max,
                    "y_low": y_low,
                    "y_high": y_high,
                    "left": left,
                    "right": right,
                    "top": plot_top,
                    "bottom": plot_bottom,
                    "color": color,
                }
            )
        canvas.create_text(
            left,
            plot_bottom + 16,
            text=f"{x_min:.3f} s",
            anchor=tk.W,
            fill=self.plot_foreground,
            font="TkDefaultFont",
        )
        canvas.create_text(
            right,
            plot_bottom + 16,
            text=f"{x_max:.3f} s",
            anchor=tk.E,
            fill=self.plot_foreground,
            font="TkDefaultFont",
        )

    def _show_hover(self, event: tk.Event) -> None:
        canvas = self.plot_canvas
        x = canvas.canvasx(event.x)
        y = canvas.canvasy(event.y)
        canvas.delete("hover")
        if not self.plot_panels:
            return
        first_panel = self.plot_panels[0]
        if not (
            first_panel["left"] <= x <= first_panel["right"]
            and first_panel["top"] <= y <= first_panel["bottom"]
        ):
            return
        x_value = first_panel["x_min"] + (x - first_panel["left"]) * (
            first_panel["x_max"] - first_panel["x_min"]
        ) / (first_panel["right"] - first_panel["left"])
        index = bisect.bisect_left(first_panel["x_values"], x_value)
        if index >= len(first_panel["x_values"]):
            index = len(first_panel["x_values"]) - 1
        elif index > 0 and abs(first_panel["x_values"][index - 1] - x_value) < abs(
            first_panel["x_values"][index] - x_value
        ):
            index -= 1

        sample_x = first_panel["x_values"][index]
        nearest: Optional[tuple[float, Dict[str, Any], float, float]] = None
        for panel in self.plot_panels:
            sample_value = panel["values"][index]
            draw_x = panel["left"] + (sample_x - panel["x_min"]) * (
                panel["right"] - panel["left"]
            ) / (panel["x_max"] - panel["x_min"])
            draw_y = panel["bottom"] - (sample_value - panel["y_low"]) * (
                panel["bottom"] - panel["top"]
            ) / (panel["y_high"] - panel["y_low"])
            candidate = (abs(draw_y - y), panel, draw_x, draw_y)
            if nearest is None or candidate[0] < nearest[0]:
                nearest = candidate
        if nearest is None:
            return
        _, panel, draw_x, draw_y = nearest
        sample_x = panel["x_values"][index]
        sample_value = panel["values"][index]
        color = panel["color"]
        canvas.create_line(
            draw_x,
            panel["top"],
            draw_x,
            panel["bottom"],
            fill=color,
            dash=(3, 3),
            tags="hover",
        )
        canvas.create_oval(
            draw_x - 4,
            draw_y - 4,
            draw_x + 4,
            draw_y + 4,
            fill=color,
            outline=color,
            tags="hover",
        )
        self.info_var.set(
            f"{panel['field']}  t={sample_x:.3f} s  "
            f"值={self._number_text(sample_value)}  "
            f"范围={self._number_text(panel['y_low'])}~"
            f"{self._number_text(panel['y_high'])}"
        )

    def _clear_hover(self, _event: Optional[tk.Event] = None) -> None:
        self.plot_canvas.delete("hover")
        test_index = self.group_label_to_index[self.group_var.get()]
        self._set_group_info(test_index)

class VentTestApp:
    def __init__(self, root: tk.Tk, profile_name: str, profile: Dict[str, Any]) -> None:
        self.root = root
        self.profile_name = profile_name
        self.profile = profile
        self.events: "queue.Queue[tuple[str, Any]]" = queue.Queue()
        self.stop = threading.Event()
        self.worker: Optional[threading.Thread] = None
        self.rows: List[Dict[str, str]] = []
        self.waveform_fields: List[str] = []
        self.collection_warnings: List[str] = []
        self.tree_items: Dict[int, str] = {}
        self.last_csv_path: Optional[Path] = None
        self.waveform_viewers: List[WaveformViewer] = []

        self.root.title("Ventcore PEEP / ΔP 自动测试")
        self.root.geometry("920x700")
        self.root.minsize(820, 620)
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self._build_ui()
        self.root.after(100, self.process_events)

    def _build_ui(self) -> None:
        top = ttk.Frame(self.root, padding=10)
        top.pack(fill=tk.X)
        ttk.Label(
            top,
            text=(
                f"设备配置：{self.profile_name}    25 组 × 10 秒    "
                "vt status 周期：250 ms"
            ),
        ).pack(side=tk.LEFT)
        ttk.Label(top, text="仅限台架测试，禁止连接患者", foreground="#b00020").pack(
            side=tk.RIGHT
        )

        buttons = ttk.Frame(self.root, padding=(10, 0, 10, 8))
        buttons.pack(fill=tk.X)
        self.start_button = ttk.Button(buttons, text="开始采集", command=self.start)
        self.start_button.pack(side=tk.LEFT)
        self.stop_button = ttk.Button(
            buttons, text="停止采集", command=self.request_stop, state=tk.DISABLED
        )
        self.stop_button.pack(side=tk.LEFT, padx=8)
        self.export_button = ttk.Button(
            buttons, text="导出 CSV", command=self.export_csv, state=tk.DISABLED
        )
        self.export_button.pack(side=tk.LEFT)
        ttk.Button(
            buttons, text="查看 CSV 波形", command=self.open_waveform_viewer
        ).pack(side=tk.LEFT, padx=8)

        self.progress = ttk.Progressbar(
            buttons, mode="determinate", maximum=25 * TEST_DURATION_SECONDS
        )
        self.progress.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(20, 0))

        table_frame = ttk.Frame(self.root, padding=(10, 0, 10, 8))
        table_frame.pack(fill=tk.BOTH, expand=True)
        self.tree = ttk.Treeview(
            table_frame,
            columns=("index", "peep", "delta", "target", "status", "samples"),
            show="headings",
            height=16,
        )
        headings = {
            "index": "组号",
            "peep": "PEEP",
            "delta": "ΔP",
            "target": "目标压力",
            "status": "状态",
            "samples": "采样点",
        }
        widths = {"index": 60, "peep": 90, "delta": 90, "target": 110, "status": 160, "samples": 90}
        for column, title in headings.items():
            self.tree.heading(column, text=title)
            self.tree.column(column, width=widths[column], anchor=tk.CENTER)
        scrollbar = ttk.Scrollbar(table_frame, orient=tk.VERTICAL, command=self.tree.yview)
        self.tree.configure(yscrollcommand=scrollbar.set)
        self.tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        test_index = 0
        for peep in PEEP_VALUES:
            for delta_pressure in DELTA_PRESSURE_VALUES:
                test_index += 1
                item = self.tree.insert(
                    "",
                    tk.END,
                    values=(
                        test_index,
                        peep,
                        delta_pressure,
                        peep + delta_pressure,
                        "等待",
                        "",
                    ),
                )
                self.tree_items[test_index] = item

        log_frame = ttk.LabelFrame(self.root, text="运行日志", padding=6)
        log_frame.pack(fill=tk.BOTH, padx=10, pady=(0, 10))
        self.log_text = tk.Text(log_frame, height=8, state=tk.DISABLED, wrap=tk.WORD)
        self.log_text.pack(fill=tk.BOTH, expand=True)

    def append_log(self, text: str) -> None:
        self.log_text.configure(state=tk.NORMAL)
        timestamp = datetime.now().strftime("%H:%M:%S")
        self.log_text.insert(tk.END, f"[{timestamp}] {text}\n")
        self.log_text.see(tk.END)
        self.log_text.configure(state=tk.DISABLED)

    def set_tree_status(self, test_index: int, status: str, samples: str = "") -> None:
        item = self.tree_items[test_index]
        values = list(self.tree.item(item, "values"))
        values[4] = status
        if samples:
            values[5] = samples
        self.tree.item(item, values=values)
        self.tree.see(item)

    def start(self) -> None:
        if not messagebox.askyesno(
            "确认开始",
            "测试将自动启动 25 组 PAC 通气。\n\n"
            "请确认设备未连接患者，并已连接合适的测试负载。",
        ):
            return
        self.rows = []
        self.waveform_fields = []
        self.collection_warnings = []
        self.stop.clear()
        self.progress["value"] = 0
        for test_index in self.tree_items:
            self.set_tree_status(test_index, "等待")
            item = self.tree_items[test_index]
            values = list(self.tree.item(item, "values"))
            values[5] = ""
            self.tree.item(item, values=values)
        self.start_button.configure(state=tk.DISABLED)
        self.stop_button.configure(state=tk.NORMAL)
        self.export_button.configure(state=tk.DISABLED)
        collector = VentTestCollector(self.profile, self.events, self.stop)
        # Keep the worker non-daemon so RTT and J-Link are always closed on exit.
        self.worker = threading.Thread(target=collector.run, daemon=False)
        self.worker.start()

    def request_stop(self) -> None:
        self.stop.set()
        self.stop_button.configure(state=tk.DISABLED)
        self.append_log("正在停止当前测试……")

    def finish(self, payload: Any, completed: bool) -> None:
        self.rows, self.waveform_fields, self.collection_warnings = payload
        self.start_button.configure(state=tk.NORMAL)
        self.stop_button.configure(state=tk.DISABLED)
        if self.rows:
            self.export_button.configure(state=tk.NORMAL)
        if completed:
            self.progress["value"] = self.progress["maximum"]
            warning_text = ""
            if self.collection_warnings:
                warning_text = (
                    f"\n发现 {len(self.collection_warnings)} 条完整性警告，"
                    "已收到的数据仍可正常导出；详情请查看运行日志。"
                )
            messagebox.showinfo(
                "采集完成",
                f"25 组测试全部完成，共采集 {len(self.rows)} 行。"
                f"{warning_text}\n请点击“导出 CSV”。",
            )

    def process_events(self) -> None:
        try:
            while True:
                event, value = self.events.get_nowait()
                if event == "log":
                    self.append_log(value)
                elif event == "test_started":
                    test_index, _, _ = value
                    self.set_tree_status(test_index, "运行中 0.0 s")
                elif event == "countdown":
                    test_index, elapsed = value
                    self.set_tree_status(test_index, f"运行中 {elapsed:.1f} s")
                    self.progress["value"] = (
                        (test_index - 1) * TEST_DURATION_SECONDS + elapsed
                    )
                elif event == "test_finished":
                    test_index, sample_count = value
                    self.set_tree_status(test_index, "完成", str(sample_count))
                    self.progress["value"] = test_index * TEST_DURATION_SECONDS
                elif event == "complete":
                    self.finish(value, True)
                elif event == "stopped":
                    self.finish(value, False)
                elif event == "error":
                    self.append_log("错误：" + value)
                    messagebox.showerror("采集失败", value)
        except queue.Empty:
            pass
        self.root.after(100, self.process_events)

    def export_csv(self) -> None:
        if not self.rows or not self.waveform_fields:
            messagebox.showwarning("无数据", "当前没有可以导出的采样数据。")
            return
        default_name = "vent_test_" + datetime.now().strftime("%Y%m%d_%H%M%S") + ".csv"
        path = filedialog.asksaveasfilename(
            title="导出测试 CSV",
            defaultextension=".csv",
            initialfile=default_name,
            filetypes=[("CSV 文件", "*.csv"), ("所有文件", "*.*")],
        )
        if not path:
            return
        fields = CSV_PREFIX_FIELDS + self.waveform_fields
        try:
            with Path(path).open("w", encoding="utf-8-sig", newline="") as handle:
                writer = csv.DictWriter(handle, fieldnames=fields)
                writer.writeheader()
                writer.writerows(self.rows)
        except OSError as exc:
            messagebox.showerror("导出失败", str(exc))
            return
        self.last_csv_path = Path(path)
        self.append_log(f"CSV 已导出：{path}")
        messagebox.showinfo("导出完成", f"已导出 {len(self.rows)} 行：\n{path}")

    def open_waveform_viewer(self) -> None:
        dialog_args: Dict[str, Any] = {
            "title": "打开测试 CSV",
            "filetypes": [("CSV 文件", "*.csv"), ("所有文件", "*.*")],
        }
        if self.last_csv_path is not None:
            dialog_args["initialdir"] = str(self.last_csv_path.parent)
            dialog_args["initialfile"] = self.last_csv_path.name
        path = filedialog.askopenfilename(**dialog_args)
        if not path:
            return
        try:
            viewer = WaveformViewer(self.root, Path(path))
        except ValueError as exc:
            messagebox.showerror("无法查看波形", str(exc))
            return
        self.last_csv_path = Path(path)
        self.waveform_viewers.append(viewer)

    def close(self) -> None:
        if self.worker is not None and self.worker.is_alive():
            if not messagebox.askyesno("退出", "采集仍在运行，确定停止并退出吗？"):
                return
            self.stop.set()
        self.root.destroy()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the Ventcore PEEP/Delta-P GUI test.")
    parser.add_argument(
        "--computer",
        help="Computer profile name from device_tool_config.json.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        config = device_tool.load_config(device_tool.DEFAULT_CONFIG_PATH)
        profile_name, profile = device_tool.select_computer(config, args.computer)
    except device_tool.DeviceToolError as exc:
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror("配置错误", str(exc))
        root.destroy()
        return 1

    root = tk.Tk()
    VentTestApp(root, profile_name, profile)
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
