"""Record a configured PAC operating point on a test lung through Device Tool RTT."""
import argparse
import csv
import hashlib
import json
import math
import re
import statistics
import time
from pathlib import Path

from test_vac_matrix import FIELDS, ROOT, Rtt


def summarize(rows, ti_ms=2000, target=30):
    """Measure the same terminal window, without counting the reference fall."""
    breaths, current = [], []
    for previous, row in zip(rows, rows[1:]):
        if (row['sequence'] != previous['sequence'] + 1 or
                row['time_ms'] != previous['time_ms'] + 6):
            raise RuntimeError('Missing waveform samples')
    for previous, row in zip(rows, rows[1:]):
        # Capture the complete inspiration, including the rising reference.
        if row['pressure_state'] == 1 and previous['pressure_state'] != 1:
            current = [row]
            continue
        if current and previous['pref_x1'] == target * 100 and row['pref_x1'] < target * 100 - 100:
            breaths.append(current)
            current = []
            continue
        if current:
            current.append(row)
    metrics = []
    # Discard two startup breaths and an incomplete final inspiration.
    for index, breath in enumerate(breaths[2:], 3):
        if abs(len(breath) * 6 - (ti_ms + 6)) > 12 or len(breath) < 43:
            raise RuntimeError('Unexpected short or incomplete PAC inspiration')
        # Reference fall is the boundary; skip 12 ms to exclude its first sample.
        tail = breath[-42:-2]
        flow = [row['prox_x2'] / 50.0 for row in tail]
        pressure = [row['ppat_x1'] / 100.0 for row in tail]
        metrics.append(dict(breath=index, tail_start_ms=tail[0]['time_ms'],
            tail_end_ms=tail[-1]['time_ms'], samples=len(tail),
            flow_min=min(flow), flow_max=max(flow),
            flow_p2p=max(flow)-min(flow), flow_sd=statistics.pstdev(flow),
            flow_mean=statistics.mean(flow), pressure_min=min(pressure),
            pressure_max=max(pressure), pressure_mean=statistics.mean(pressure),
            pressure_p2p=max(pressure)-min(pressure),
            flow_signal_valid=not (
                max(row['air_x2'] for row in breath)-min(row['air_x2'] for row in breath) <= 1 and
                max(row['prox_x2'] for row in breath)-min(row['prox_x2'] for row in breath) > 500)))
        # Compare early/late hold windows after 60 ms of actuator settling.
        hold = [row for row in breath if row['pressure_state'] == 2]
        plateau = hold[10:-2]
        if len(plateau) >= 20:
            early = statistics.mean(row['ppat_x1'] / 100 for row in plateau[:10])
            late = statistics.mean(row['ppat_x1'] / 100 for row in plateau[-10:])
            metrics[-1].update(plateau_early=early, plateau_late=late,
                plateau_drift=late-early,
                plateau_undershoot=max(0.0, target - min(row['ppat_x1'] for row in plateau) / 100),
                plateau_p2p=(max(row['ppat_x1'] for row in plateau) -
                             min(row['ppat_x1'] for row in plateau)) / 100)
        # Fixed time windows also include the filling-tail undershoot.
        half = breath[-(ti_ms // 12 + 2):-2]
        steady = breath[max(0, len(breath) - 102):-2]
        half_flow = [row['prox_x2'] / 50.0 for row in half]
        steady_flow = [row['prox_x2'] / 50.0 for row in steady]
        metrics[-1].update(half_flow_min=min(half_flow),
            peak_pressure=max(row['ppat_x1'] for row in breath) / 100,
            overshoot=max(row['ppat_x1'] for row in breath) / 100 - target,
            half_reverse_ml=sum(max(0, -v) for v in half_flow) * 0.1,
            half_pressure_min=min(row['ppat_x1'] for row in half) / 100,
            half_pressure_max=max(row['ppat_x1'] for row in half) / 100,
            steady_flow_p2p=max(steady_flow)-min(steady_flow),
            steady_flow_sd=statistics.pstdev(steady_flow),
            steady_flow_mean=statistics.mean(steady_flow))
    if not metrics:
        raise RuntimeError('Insufficient complete PAC breaths')
    plateau_metrics = [row for row in metrics if 'plateau_drift' in row]
    return dict(window='40 samples (240 ms), ending 12 ms before reference fall',
        excluded_startup_breaths=2, breaths=metrics,
        mean_peak_pressure=statistics.mean(row['peak_pressure'] for row in metrics),
        max_peak_pressure=max(row['peak_pressure'] for row in metrics),
        mean_pressure_p2p=statistics.mean(row['pressure_p2p'] for row in metrics),
        flow_signal_valid=all(row['flow_signal_valid'] for row in metrics),
        max_abs_plateau_drift=max(abs(row['plateau_drift']) for row in plateau_metrics) if plateau_metrics else None,
        mean_plateau_drift=statistics.mean(row['plateau_drift'] for row in plateau_metrics) if plateau_metrics else None,
        mean_plateau_p2p=statistics.mean(row['plateau_p2p'] for row in plateau_metrics) if plateau_metrics else None,
        max_plateau_undershoot=max(row['plateau_undershoot'] for row in plateau_metrics) if plateau_metrics else None,
        mean_overshoot=statistics.mean(row['overshoot'] for row in metrics),
        mean_flow_p2p=statistics.mean(row['flow_p2p'] for row in metrics),
        max_flow_p2p=max(row['flow_p2p'] for row in metrics),
        mean_flow_sd=statistics.mean(row['flow_sd'] for row in metrics),
        half_window_ms=(ti_ms // 12) * 6,
        mean_half_flow_min=statistics.mean(row['half_flow_min'] for row in metrics),
        worst_half_flow_min=min(row['half_flow_min'] for row in metrics),
        mean_half_reverse_ml=statistics.mean(row['half_reverse_ml'] for row in metrics),
        half_pressure_min=min(row['half_pressure_min'] for row in metrics),
        half_pressure_max=max(row['half_pressure_max'] for row in metrics),
        steady_window_ms=min(600, (len(breaths[2]) - 2) * 6),
        mean_steady_flow_p2p=statistics.mean(row['steady_flow_p2p'] for row in metrics),
        mean_steady_flow_sd=statistics.mean(row['steady_flow_sd'] for row in metrics),
        pressure_min=min(row['pressure_min'] for row in metrics),
        pressure_max=max(row['pressure_max'] for row in metrics))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=float, default=68)
    parser.add_argument('--max-plateau-drift', type=float, default=None)
    parser.add_argument('--max-plateau-undershoot', type=float, default=None)
    parser.add_argument('--peep', type=int, default=5)
    parser.add_argument('--delta', type=int, default=25)
    parser.add_argument('--ti-ms', type=int, default=800)
    parser.add_argument('--rate', type=int, default=25)
    parser.add_argument('--rise-ms', type=int, default=200)
    args = parser.parse_args()
    if args.max_plateau_drift is not None and (
            not math.isfinite(args.max_plateau_drift) or args.max_plateau_drift < 0):
        parser.error('Plateau drift limit must be finite and nonnegative')
    if args.max_plateau_undershoot is not None and (
            not math.isfinite(args.max_plateau_undershoot) or args.max_plateau_undershoot < 0):
        parser.error('Plateau undershoot limit must be finite and nonnegative')
    target = args.peep + args.delta
    if not (1 <= args.peep < target <= 100 and 1 <= args.rate <= 160 and
            0 <= args.rise_ms <= args.ti_ms <= 60000):
        parser.error('Invalid PAC pressure or timing')
    if args.seconds < 32:
        parser.error('Use at least 32 seconds for repeated PAC breaths')
    args.output.mkdir(parents=True, exist_ok=True)
    ti_ms = args.ti_ms
    metadata = dict(peep=args.peep, delta=args.delta, target_pressure=target, trigger='off', ti_ms=ti_ms, rate=args.rate, rise_ms=args.rise_ms,
        note='PAC timing explicitly configured through vt set.',
        seconds=args.seconds, max_plateau_drift=args.max_plateau_drift,
        max_plateau_undershoot=args.max_plateau_undershoot, hashes={})
    for name in ['build/Debug/ventcore.hex', 'User/app/ventalgo/pressurecontroller.c',
                 'User/app/ventalgo/pressurecontroller.h', 'User/app/databus/settingdata.c']:
        metadata['hashes'][name] = hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
    (args.output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    rows = []
    rtt = Rtt(args.output / 'rtt.log')
    try:
        rtt.until('RTT connected', timeout=20)
        for attempt in range(5):
            rtt.send('help')
            try:
                rtt.until('Ventilation test', timeout=2)
                break
            except TimeoutError:
                if attempt == 4:
                    raise
        rtt.command('vt stop', 'stop status=')
        time.sleep(1)
        rtt.command(f'vt set {args.peep} {args.delta} {ti_ms} {args.rate} {args.rise_ms}', f'target100={target * 100} status=1')
        rtt.command('vt trigger off', 'trigger mode=1 type=0 threshold100=0 status=1')
        rtt.status(allow_dropped=True)
        rtt.command('vt pac', 'PAC start status=1')
        start = time.monotonic()
        while time.monotonic() - start < args.seconds:
            if (args.output / 'stop').exists():
                raise RuntimeError('Bench stop requested')
            tick = time.monotonic()
            rows.extend(rtt.status())
            time.sleep(max(0, .25 - (time.monotonic() - tick)))
        rows.extend(rtt.status())
    finally:
        try:
            rtt.close()
        finally:
            with (args.output / 'waveform.csv').open('w', newline='') as handle:
                writer = csv.DictWriter(handle, fieldnames=FIELDS)
                writer.writeheader()
                writer.writerows(rows)
    result = summarize(rows, ti_ms, target)
    observed = re.findall(r'VT_BREATH_RESULT,sequence=\d+,mode=(\d+),.*?ti_ms=(\d+)',
                          (args.output / 'rtt.log').read_text(encoding='utf-8'))
    settings_seen = re.findall(
        r'VT_PAC_SETTINGS,peep100=(\d+),delta100=(\d+),rate100=(\d+),ti_ms=(\d+),rise_ms=(\d+)',
        (args.output / 'rtt.log').read_text(encoding='utf-8'))
    expected = (args.peep * 100, args.delta * 100, args.rate * 100, ti_ms, args.rise_ms)
    result['settings_valid'] = bool(settings_seen) and all(
        tuple(map(int, values)) == expected for values in settings_seen)
    result['observed_ti_ms'] = sorted({int(ti) for mode, ti in observed})
    # Monitor Ti extends to negative-flow confirmation; use reference timing here.
    start_ms = None
    control_ti = []
    for previous, row in zip(rows, rows[1:]):
        if row['pressure_state'] == 1 and previous['pressure_state'] != 1:
            start_ms = row['time_ms']
        if (start_ms is not None and previous['pref_x1'] == target * 100 and
                row['pref_x1'] < target * 100 - 100):
            control_ti.append(row['time_ms'] - start_ms - 6)
            start_ms = None
    result['control_ti_ms'] = sorted(set(control_ti))
    result['default_timing_valid'] = (bool(observed) and bool(control_ti) and
        all(int(mode) == 1 for mode, ti in observed) and
        all(ti_ms <= ti <= ti_ms + 6 for ti in control_ti))
    # Report flow amplitudes for comparison; do not equate a capture with no oscillation.
    result['capture_valid'] = len(result['breaths']) >= 10
    # The half-inspiration window may still include the effective high-pressure rise.
    result['pressure_within_bench_band'] = (result['pressure_min'] >= target - 2 and
                                          result['max_peak_pressure'] <= target + 2)
    result['plateau_flat'] = (args.max_plateau_drift is None or
        (result['max_abs_plateau_drift'] is not None and
         result['max_abs_plateau_drift'] <= args.max_plateau_drift))
    result['plateau_undershoot_valid'] = (args.max_plateau_undershoot is None or
        (result['max_plateau_undershoot'] is not None and
         result['max_plateau_undershoot'] <= args.max_plateau_undershoot))
    (args.output / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'breaths'}), flush=True)
    if not all(result[key] for key in ('capture_valid', 'flow_signal_valid', 'plateau_flat', 'plateau_undershoot_valid', 'settings_valid', 'pressure_within_bench_band',
                                       'default_timing_valid')):
        raise RuntimeError('Invalid PAC capture or pressure outside bench band; inspect waveforms')


if __name__ == '__main__':
    main()
