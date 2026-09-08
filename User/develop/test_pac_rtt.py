"""Record default PAC on a confirmed test lung through Device Tool RTT."""
import argparse
import csv
import hashlib
import json
import statistics
import time
from pathlib import Path

from test_vac_matrix import FIELDS, ROOT, Rtt


def summarize(rows):
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
        if len(breath) < 67:
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
    if not metrics:
        raise RuntimeError('Insufficient complete PAC breaths')
    return dict(window='40 samples (240 ms), ending 12 ms before reference fall',
        excluded_startup_breaths=2, breaths=metrics,
        mean_flow_p2p=statistics.mean(row['flow_p2p'] for row in metrics),
        max_flow_p2p=max(row['flow_p2p'] for row in metrics),
        mean_flow_sd=statistics.mean(row['flow_sd'] for row in metrics),
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
    metadata = dict(peep=5, delta=25, target_pressure=30, trigger='off',
        note='Reset firmware first: default Ti 1350 ms, rate 20/min, oxygen 21%.',
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
    result = summarize(rows)
    # Report flow amplitudes for comparison; do not equate a capture with no oscillation.
    result['capture_valid'] = len(result['breaths']) >= 10
    result['pressure_within_bench_band'] = (result['pressure_min'] >= 28 and
                                          result['pressure_max'] <= 32)
    (args.output / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'breaths'}), flush=True)
    if not result['capture_valid'] or not result['pressure_within_bench_band']:
        raise RuntimeError('Invalid PAC capture or pressure outside bench band; inspect waveforms')


if __name__ == '__main__':
    main()
