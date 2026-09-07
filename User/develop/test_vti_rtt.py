"""Record startup VTI convergence on a test lung through Device Tool RTT."""
import argparse
import csv
import hashlib
import json
import re
import time
from pathlib import Path

from test_vac_matrix import FIELDS, ROOT, Rtt


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=float, default=44)
    parser.add_argument('--peep', type=int, default=5)
    parser.add_argument('--pause', type=int, default=0)
    args = parser.parse_args()
    if not 0 <= args.pause <= 99 or args.seconds < 12 or not 1 <= args.peep <= 25:
        parser.error('Use pause 0..99, seconds >=12 and PEEP 1..25')
    args.output.mkdir(parents=True, exist_ok=True)
    metadata = dict(peep=args.peep, pause=args.pause, target_ml=500,
                    seconds=args.seconds,
                    scheduler_header=(ROOT/'User/app/ventlogic/breathscheduler.h').read_text(encoding='utf-8'),
                    scheduler_sha256=hashlib.sha256((ROOT/'User/app/ventlogic/breathscheduler.c').read_bytes()).hexdigest(),
                    firmware_sha256=hashlib.sha256((ROOT/'build/Debug/ventcore.hex').read_bytes()).hexdigest())
    (args.output/'metadata.json').write_text(json.dumps(metadata, indent=2)+'\n', encoding='utf-8')
    rtt = Rtt(args.output/'rtt.log')
    rows, breaths, feedback = [], {}, {}
    start = None

    def status(discard=False):
        rtt.send('vt status')
        lines = rtt.until('VT_TRANSIENT_END')
        for line in lines:
            if 'VT_TRANSIENT_BEGIN' in line:
                dropped = re.search(r'dropped=(\d+)', line)
                if dropped and int(dropped[1]) and not discard:
                    raise RuntimeError('Lost waveform samples: '+line)
            for tag, collection in [('VT_BREATH_RESULT,', breaths), ('VT_VOLUME_FEEDBACK,', feedback)]:
                if tag in line and start is not None:
                    values = dict(item.split('=', 1) for item in line.split(tag, 1)[1].split(','))
                    key = int(values['sequence'])
                    if key not in collection:
                        values['received_s'] = round(time.monotonic()-start, 3)
                        collection[key] = values
                        if tag.startswith('VT_BREATH'):
                            print(json.dumps(values), flush=True)
            parts = line.split(',')
            if not discard and len(parts) == len(FIELDS) and all(p.lstrip('-').isdigit() for p in parts):
                rows.append(dict(zip(FIELDS, map(int, parts))))

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
        rtt.command(f'vt volume {args.peep} 500 {args.pause}', 'volume peep=')
        status(discard=True)
        start = time.monotonic()
        rtt.command('vt run 1', 'run 1 status=')
        while time.monotonic()-start < args.seconds:
            if (args.output/'stop').exists():
                raise RuntimeError('Bench stop requested')
            poll = time.monotonic()
            status()
            time.sleep(max(0, 0.25-(time.monotonic()-poll)))
        status()
    finally:
        try:
            rtt.close()
        finally:
            with (args.output/'waveform.csv').open('w', newline='', encoding='utf-8') as handle:
                writer = csv.DictWriter(handle, fieldnames=FIELDS)
                writer.writeheader()
                writer.writerows(rows)
            result = dict(breaths=list(breaths.values()), feedback=list(feedback.values()))
            (args.output/'results.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    for previous, current in zip(rows, rows[1:]):
        if current['sequence'] != previous['sequence']+1 or current['time_ms'] != previous['time_ms']+6:
            raise RuntimeError('Discontinuous waveform')
    if len(breaths) < 2:
        raise RuntimeError('Insufficient completed breaths')
    print('Saved '+str(args.output), flush=True)


if __name__ == '__main__':
    main()
