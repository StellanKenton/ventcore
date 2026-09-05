"""Collect VAC bench waveforms through the Device Tool RTT entry point."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import queue
import re
import statistics
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[2]
FIELDS = "sequence time_ms air_x2 o2_x2 prox_x2 pinsp_x1 ppeep_x1 pexp_x1 ppat_x1 blower_x10 pref_x1 flowcomp_x1 pcorr_x1 effort_x1 ff_x1 vt_x10 vti_x10 vte_x10 target_x100 valve_x2 expiration_state pressure_state volume_pause pause_settled leak_lpm".split()


def crossings(values, threshold=0.0):
    previous = 0
    count = 0
    for value in values:
        sign = 1 if value > threshold else -1 if value < -threshold else 0
        if sign:
            count += int(previous != 0 and previous != sign)
            previous = sign
    return count


def summarize(rows):
    pauses = []
    current = []
    previous = None
    for row in rows:
        if previous is not None and (row["sequence"] != previous["sequence"] + 1 or
                                     row["time_ms"] != previous["time_ms"] + 6):
            raise RuntimeError("Missing or discontinuous waveform samples")
        previous = row
        if row["volume_pause"]:
            if current and row["sequence"] != current[-1]["sequence"] + 1:
                raise RuntimeError("Sequence gap inside pause")
            current.append(row)
        elif current:
            pauses.append(current)
            current = []
    result = []
    # Ignore two breaths after changing the operating point and any trailing pause.
    for index, pause in enumerate(pauses[2:], 3):
        flow = [r["prox_x2"] / 50 for r in pause]
        tail = flow[20:]
        if len(pause) < 150:
            continue
        result.append(dict(breath=index, samples=len(pause),
            crossings=crossings(flow), crossings_05=crossings(flow, 0.5),
            tail_crossings=crossings(tail), tail_crossings_05=crossings(tail, 0.5),
            tail_min=min(tail), tail_max=max(tail),
            tail_rms=(statistics.mean(v*v for v in tail))**0.5,
            pause_net_ml=sum(flow)*0.1,
            end_vti_ml=pause[-1]["vti_x10"]/10,
            pressure_min=min(r["ppat_x1"] for r in pause)/100,
            pressure_max=max(r["ppat_x1"] for r in pause)/100))
        steady = [r for r in pause if r["time_ms"] - pause[0]["time_ms"] >= 600]
        steady_flow = [r["prox_x2"]/50 for r in steady]
        result[-1].update(steady_samples=len(steady),
            steady_flow_sd=statistics.pstdev(steady_flow),
            steady_flow_p2p=max(steady_flow)-min(steady_flow))
        if "leak_lpm" in pause[0]:
            leak = [max(0, min(120, r["leak_lpm"]/100)) for r in steady]
            error = [flow - target for flow, target in zip(steady_flow, leak)]
            result[-1].update(steady_settled=all(r["pause_settled"] for r in steady),
                steady_leak_mean=statistics.mean(leak),
                steady_error_mean=statistics.mean(error),
                steady_error_rms=statistics.mean(v*v for v in error)**0.5)
    return result


class Rtt:
    def __init__(self, path):
        self.log = path.open("w", encoding="utf-8")
        self.lines = queue.Queue()
        self.process = subprocess.Popen(
            ["py", "-3", "-u", "user/develop/quick_deploy.py", "rtt"],
            cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, bufsize=1)
        self.reader = threading.Thread(target=self.read, daemon=True)
        self.reader.start()

    def read(self):
        for line in self.process.stdout:
            self.log.write(line)
            self.log.flush()
            self.lines.put(line.strip())

    def send(self, command):
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def until(self, marker, timeout=5):
        lines = []
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            try:
                line = self.lines.get(timeout=0.1)
            except queue.Empty:
                continue
            lines.append(line)
            if marker in line:
                return lines
        raise TimeoutError(f"RTT timeout waiting for {marker}: {lines[-5:]}")

    def command(self, command, marker):
        self.send(command)
        lines = self.until(marker)
        if "status=1" not in lines[-1]:
            raise RuntimeError(lines[-1])
        print(lines[-1], flush=True)

    def status(self, allow_dropped=False):
        self.send("vt status")
        lines = self.until("VT_TRANSIENT_END")
        rows = []
        for line in lines:
            if "VT_TRANSIENT_BEGIN" in line:
                dropped = re.search(r"dropped=(\d+)", line)
                if dropped and int(dropped[1]):
                    if not allow_dropped:
                        raise RuntimeError(f"Dropped waveform samples: {line}")
                    print(f"NOTICE: discarded pre-test backlog: {line}", flush=True)
            parts = line.split(",")
            if len(parts) == len(FIELDS) and all(p.lstrip("-").isdigit() for p in parts):
                rows.append(dict(zip(FIELDS, map(int, parts))))
        return rows

    def close(self):
        try:
            self.command("vt stop", "stop status=")
        finally:
            # Stop the Device Tool process tree, including its J-Link child.
            subprocess.run(["taskkill", "/PID", str(self.process.pid), "/T", "/F"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.process.wait(timeout=5)
            self.reader.join(timeout=5)
            self.log.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--seconds", type=float, default=29)
    parser.add_argument("--peep", type=int, nargs="+", default=[5, 10, 15])
    parser.add_argument("--volume", type=int, nargs="+", default=[300, 500, 700])
    args = parser.parse_args()
    if args.seconds < 24 or any(p not in (5, 10, 15) for p in args.peep) or any(
            v not in (300, 500, 700) for v in args.volume):
        parser.error("Use at least 24 seconds and PEEP 5/10/15, volume 300/500/700")
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "metadata.json").write_text(json.dumps(dict(
        seconds=args.seconds, peep=args.peep, volume=args.volume,
        controller_header=(ROOT / "User/app/ventalgo/flowcontroller.h").read_text(),
        controller_sha256=hashlib.sha256((ROOT / "User/app/ventalgo/flowcontroller.c").read_bytes()).hexdigest(),
        firmware_sha256=hashlib.sha256((ROOT / "build/Debug/ventcore.hex").read_bytes()).hexdigest()
    ), indent=2)+"\n", encoding="utf-8")
    rtt = Rtt(args.output / "rtt.log")
    all_summary = []
    try:
        rtt.until("RTT connected", timeout=20)
        for attempt in range(5):
            rtt.send("help")
            try:
                rtt.until("Ventilation test", timeout=2)
                break
            except TimeoutError:
                if attempt == 4:
                    raise
        for peep in args.peep:
            for volume in args.volume:
                rtt.command(f"vt volume {peep} {volume} 50", "volume peep=")
                rtt.status(allow_dropped=True)
                rtt.command("vt run 1", "run 1 status=")
                start = time.monotonic()
                rows = []
                while time.monotonic() - start < args.seconds:
                    if (args.output / "stop").exists():
                        raise KeyboardInterrupt("Bench collection stop requested")
                    poll = time.monotonic()
                    rows.extend(rtt.status())
                    time.sleep(max(0, 0.25 - (time.monotonic()-poll)))
                rtt.command("vt stop", "stop status=")
                rows.extend(rtt.status())
                with (args.output / f"p{peep}_v{volume}.csv").open("w", newline="", encoding="utf-8") as handle:
                    writer = csv.DictWriter(handle, fieldnames=FIELDS)
                    writer.writeheader()
                    writer.writerows(rows)
                metrics = summarize(rows)
                if len(metrics) < 3:
                    raise RuntimeError(f"Only {len(metrics)} complete steady breaths")
                for metric in metrics:
                    metric.update(peep=peep, volume=volume)
                all_summary.extend(metrics)
                (args.output / "summary.json").write_text(json.dumps(all_summary, indent=2)+"\n", encoding="utf-8")
                print(json.dumps(dict(peep=peep, volume=volume, breaths=len(metrics),
                    crossings=[m["crossings"] for m in metrics],
                    crossings_05=[m["crossings_05"] for m in metrics],
                    rms=round(statistics.mean(m["tail_rms"] for m in metrics), 2),
                    steady_sd=round(statistics.mean(m["steady_flow_sd"] for m in metrics), 2),
                    error_mean=round(statistics.mean(m["steady_error_mean"] for m in metrics), 2))), flush=True)
    finally:
        rtt.close()


if __name__ == "__main__":
    main()
