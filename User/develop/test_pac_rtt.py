"""Record default PAC on a confirmed test lung through Device Tool RTT."""
import argparse
import csv
import hashlib
import json
import re
import statistics
import time
from pathlib import Path

from test_vac_matrix import FIELDS, ROOT, Rtt


def summarize(rows, ti_ms=2000):
    """Measure the same terminal window, without counting the reference fall."""
    breaths, current = [], []
    for previous, row in zip(rows, rows[1:]):
        if (row['sequence'] != previous['sequence'] + 1 or
                row['time_ms'] != previous['time_ms'] + 6):
            raise RuntimeError('Missing waveform samples')
    for row in rows:
        if current and row['pref_x1'] < 2900:
            breaths.append(current)
            current = []
        if row['pref_x1'] >= 2900:
            current.append(row)
    metrics = []
    # Discard two startup breaths and an incomplete final inspiration.
    for index, breath in enumerate(breaths[2:], 3):
        if len(breath) < max(102, ti_ms // 12 + 2):
            raise RuntimeError('Unexpected short PAC plateau')
        # Reference fall is the boundary; skip 12 ms to exclude its first sample.
        tail = breath[-42:-2]
        flow = [row['prox_x2'] / 50.0 for row in tail]
        pressure = [row['ppat_x1'] / 100.0 for row in tail]
        metrics.append(dict(breath=index, tail_start_ms=tail[0]['time_ms'],
            tail_end_ms=tail[-1]['time_ms'], samples=len(tail),
            flow_min=min(flow), flow_max=max(flow),
            flow_p2p=max(flow)-min(flow), flow_sd=statistics.pstdev(flow),
            flow_mean=statistics.mean(flow), pressure_min=min(pressure),
            pressure_max=max(pressure), pressure_mean=statistics.mean(pressure)))
        # Fixed time windows also include the filling-tail undershoot.
        half = breath[-(ti_ms // 12 + 2):-2]
        steady = breath[-102:-2]
        half_flow = [row['prox_x2'] / 50.0 for row in half]
        steady_flow = [row['prox_x2'] / 50.0 for row in steady]
        metrics[-1].update(half_flow_min=min(half_flow),
            half_reverse_ml=sum(max(0, -v) for v in half_flow) * 0.1,
            half_pressure_min=min(row['ppat_x1'] for row in half) / 100,
            half_pressure_max=max(row['ppat_x1'] for row in half) / 100,
            steady_flow_p2p=max(steady_flow)-min(steady_flow),
            steady_flow_sd=statistics.pstdev(steady_flow),
            steady_flow_mean=statistics.mean(steady_flow))
    if not metrics:
        raise RuntimeError('Insufficient complete PAC breaths')
    return dict(window='40 samples (240 ms), ending 12 ms before reference fall',
        excluded_startup_breaths=2, breaths=metrics,
        mean_flow_p2p=statistics.mean(row['flow_p2p'] for row in metrics),
        max_flow_p2p=max(row['flow_p2p'] for row in metrics),
        mean_flow_sd=statistics.mean(row['flow_sd'] for row in metrics),
        half_window_ms=(ti_ms // 12) * 6,
        mean_half_flow_min=statistics.mean(row['half_flow_min'] for row in metrics),
        worst_half_flow_min=min(row['half_flow_min'] for row in metrics),
        mean_half_reverse_ml=statistics.mean(row['half_reverse_ml'] for row in metrics),
        half_pressure_min=min(row['half_pressure_min'] for row in metrics),
        half_pressure_max=max(row['half_pressure_max'] for row in metrics),
        steady_window_ms=600,
        mean_steady_flow_p2p=statistics.mean(row['steady_flow_p2p'] for row in metrics),
        mean_steady_flow_sd=statistics.mean(row['steady_flow_sd'] for row in metrics),
        pressure_min=min(row['pressure_min'] for row in metrics),
        pressure_max=max(row['pressure_max'] for row in metrics))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=float, default=68)
    args = parser.parse_args()
    if args.seconds < 44:
        parser.error('Use at least 44 seconds for repeated PAC breaths')
    args.output.mkdir(parents=True, exist_ok=True)
    settings = (ROOT / 'User/app/databus/settingdata.c').read_text(encoding='utf-8')
    pac = settings.split('gVentPacSettings = {', 1)[1].split('};', 1)[0]
    ti_ms = int(re.search(r'\.inspiratoryTimeMs\s*=\s*(\d+)', pac)[1])
    metadata = dict(peep=5, delta=25, target_pressure=30, trigger='off', ti_ms=ti_ms,
        note='Reset firmware first; Ti is read from current PAC source defaults.',
        seconds=args.seconds, hashes={})
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
        rtt.command('vt set 5 25', 'target100=3000 status=1')
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
    result = summarize(rows, ti_ms)
    observed = re.findall(r'VT_BREATH_RESULT,sequence=\d+,mode=(\d+),.*?ti_ms=(\d+)',
                          (args.output / 'rtt.log').read_text(encoding='utf-8'))
    result['observed_ti_ms'] = sorted({int(ti) for mode, ti in observed})
    # Monitor Ti extends to negative-flow confirmation; use reference timing here.
    start_ms = None
    control_ti = []
    for previous, row in zip(rows, rows[1:]):
        if row['pressure_state'] == 1 and previous['pressure_state'] != 1:
            start_ms = row['time_ms']
        if (start_ms is not None and previous['pref_x1'] == 3000 and
                row['pref_x1'] < 2900):
            control_ti.append(row['time_ms'] - start_ms - 6)
            start_ms = None
    result['control_ti_ms'] = sorted(set(control_ti))
    result['default_timing_valid'] = (bool(observed) and bool(control_ti) and
        all(int(mode) == 1 for mode, ti in observed) and
        all(ti_ms <= ti <= ti_ms + 6 for ti in control_ti))
    # Report flow amplitudes for comparison; do not equate a capture with no oscillation.
    result['capture_valid'] = len(result['breaths']) >= 10
    result['pressure_within_bench_band'] = (result['half_pressure_min'] >= 28 and
                                          result['half_pressure_max'] <= 32)
    (args.output / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'breaths'}), flush=True)
    if not all(result[key] for key in ('capture_valid', 'pressure_within_bench_band',
                                       'default_timing_valid')):
        raise RuntimeError('Invalid PAC capture or pressure outside bench band; inspect waveforms')


if __name__ == '__main__':
    main()
