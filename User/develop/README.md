# Device Tool

`test_protocol.py` also verifies half-up rounding of total/mandatory/spontaneous
frequency at 14.49, 14.5 and 14.51 breaths/min, plus integer and upper-limit cases.

Expiratory peak flow regressions: `test_monitor_leak.py` checks phase/sign
selection, peak hold, zero flow, invalid samples and reset; `test_protocol.py`
checks MCM 0x0F encoding, CRC, once-per-breath upload and invalid suppression.

Minute ventilation regressions: `test_monitor_leak.py` checks inspiratory and
expiratory L/min, zero flow, invalid flow, zero duration, tick wrap and stop reset.
`test_protocol.py` checks MCM 0x08/0x09 decimal encoding, saturation, CRC,
once-per-breath publication and invalid-result suppression.

`device_tool.py` is a local development helper for this firmware project. It reads
`device_tool_config.json`, matches the current computer by OS and hostname, then
uses the matched profile to build, flash, reset, and read RTT logs from the
target board.

## Files

- `test_rtos_timing.py`: host regression using production `portrtos.c` with kernel
  stubs at the configured 1000 Hz tick rate. Covers periodic delays across the
  71-minute multiplication-overflow boundary, captured uptime, 24 hours and
  32-bit tick wrap, plus relative delays and invalid arguments. Run
  `py -3 user/develop/test_rtos_timing.py`; does not operate the board.

- `ventcore.jdebug`: Ozone application debug project for `build/Debug/ventcore.elf`.
  Builds and normal flashing/reset/RTT operations use Device Tool as above.
  Ozone reset/download callbacks initialize SP, PC and VTOR from the application
  vectors at `0x08010000`; do not leave these callbacks empty. This avoids running
  a stale image at `0x08000000`. Reset strategy 0 uses SYSRESETREQ on this target;
  the reset-pin strategy did not clear the captured HardFault on the current board.
  It does not install or repair a bootloader:
  standalone power-on/reset still requires a valid bootloader that hands off to
  `0x08010000`. Use this project for application startup debugging; attach to an
  already running target when the bootloader handoff itself must be inspected.

- `test_trigger.py`: host regression for the production trigger engine. Run
  `py -3 user/develop/test_trigger.py`. Covers passive negative-flow recovery,
  positive bias flow, real flow/pressure efforts, consecutive confirmation,
  PEEP settling, low-PEEP rearming across breaths, real release/capture-controller
  recovery after pressure undershoot, consecutive release dwell, high-pressure
  blocking, pressure overshoot recovery, invalid samples/thresholds and
  disabled/unsupported modes, including VAC flow/pressure triggering and trigger off.
  Covers a 300 ms VAC flow ramp to 23.2 L/min at a 10 L/min threshold,
  established positive bias, subthreshold ramps, and bias reset/decay across breaths.
  Pressure cases include a 300 ms fall from 2.5 to 0 cmH2O with a 2 cmH2O
  threshold, low-pressure rebound, threshold confirmation and per-breath reset.
  Pressure references continue updating in stable windows. Signed flow references
  follow negative expiration; tests cover efforts before zero crossing, linear and
  exponential passive emptying, readiness gating and trigger-type changes.
  Uses a native GCC or Clang; does not operate hardware.

- `test_flow_conversion.py`: host regression using production calibration and data
  processing with asymmetric nonlinear flow tables. Checks table knots, ADC zero
  drift in both directions, repeated zeroing, SFM3119-to-BTPS conversion,
  pressure-density correction at table knots and through the real filters,
  and unavailable-table recovery.

- `test_pac_rtt.py`: PAC pressure/flow bench regression through Device Tool RTT.
  With a test lung connected, build/flash using Device Tool, then run
  `py -3 user/develop/test_pac_rtt.py --output build/pac_check --delta 45 --seconds 44`.
  Defaults explicitly configure PEEP 5, rate 25/min, Ti 800 ms, rise setting
  200 ms and trigger off; `--peep`, `--delta`, `--ti-ms`, `--rate`, `--rise-ms` select the
  operating point. `vt set <peep> <delta> [ti_ms rate rise_ms]` preserves timing
  when the optional triple is omitted and rejects malformed/impossible timing.
  `VT_PAC_SETTINGS` confirms the applied values during collection.
  The script stops ventilation on completion/error. An empty `stop` file in the
  output directory aborts collection. Save each run in a separate directory.
  RTT, continuous 6 ms CSV, source/firmware hashes and per-breath metrics are saved.
  Two startup breaths are excluded; at least ten complete analyzed breaths are
  required. The terminal window is 240 ms, ending 12 ms before reference fall.
  For the PEEP 10 platform-ramp regression, run
  `py -3 user/develop/test_pac_rtt.py --output build/pac_flat_check --peep 10 --delta 25 --rate 15 --seconds 68 --max-plateau-drift 0.8`.
  Plateau metrics exclude the first 60 ms of HOLD and final 12 ms of inspiration;
  drift is the last 60 ms mean minus the first 60 ms mean of that window.
  The optional drift limit checks every analyzed breath's absolute drift;
  without this option, `plateau_flat=true` means the check was not requested.
  Plateau peak-to-peak also reports intermediate dips that drift alone can miss.
  `--max-plateau-undershoot 1.0` additionally rejects any analyzed plateau
  sample more than 1 cmH2O below target, including an intermediate dip even if
  the beginning and end agree. It uses the same HOLD window as plateau drift.
  Example: `--peep 15 --delta 25 --rate 15 --seconds 56 --max-plateau-drift 0.8 --max-plateau-undershoot 1.0`.
  Without this optional limit, `plateau_undershoot_valid=true` means that check
  was not requested. Both optional limits must be finite and nonnegative.
  A frozen supply-flow signal during changing patient flow invalidates a capture.
  Bench acceptance requires terminal pressure within target +/-2 cmH2O and
  full-inspiration peak no more than target +2, as well as matching settings,
  continuous samples and control Ti. This is a bench check, not a clinical limit.
  The reported half-inspiration window can include the effective high-pressure
  rise, so its minimum is not used as a settled-pressure acceptance criterion.
  Flow peak-to-peak includes normal decelerating flow; inspect waveforms before
  interpreting it as oscillation. Disconnect other RTT readers (including Ozone)
  during capture: readers can consume one another's log bytes.
  `test_flow_pause.py` also verifies PAC high-flow compensation, bounded feedback
  at targets 20/30/40/50, low-flow handoff, saturation and PSV/ST isolation.
  It also covers PEEP compensation interpolation, pressure capture before applying
  falling-flow advance, per-breath reset and the unchanged PEEP 5 response.
  The filling-tail brake tests verify deceleration-only output, its bound,
  pressure gating, decay without a retained PI bias, and invalid-speed recovery.

- `test_protocol.py`: host regression using the production MCM parser, caches and settings binding with a simulated UART. Run `py -3 user/develop/test_protocol.py`; covers MCM alarm limits with local/host settings, scaling, partial updates and CRC rejection, physiological alarm wire bits, recovery and unchanged-state suppression, fragmented frames, malformed packets, CRC, ACK, source switching, ventilation commands and waveform encoding, plus 1110 heartbeat responses with continuous traffic, bursts, UART busy/queue backpressure, timeout and reconnect. It does not flash or operate the board.

- `test_vti_rtt.py`: startup convergence recording through Device Tool RTT on a test
  lung. `py -3 user/develop/test_vti_rtt.py --output build/vti_rtt/run --seconds 44
  --peep 5 --pause 0` starts VAC at 500 mL, records every result/feedback and 6 ms
  waveform, and stops on completion. Create `stop` in the output directory to abort.

- `test_vac_matrix.py`: simulated-lung VAC matrix collection through
  `quick_deploy.py rtt`. Run with `--output build/vac_baseline`; defaults to
  PEEP 5/10/15 and volume 300/500/700, 29 seconds per group. Saves raw RTT,
  6 ms CSV waveforms and per-breath pause metrics after discarding two breaths.
  `vt volume <peep> <ml> [pause_pct]` configures VAC; `vt run 1` starts it.
  Pause accepts 0..99 percent; omission preserves the current setting and the
  power-on default is 0. This pause matrix explicitly requests 50 percent.
  `vt volume 15 500 0` selects a full delivery interval without pause. `volume_pause`
  marks the exact pause interval; `pause_settled` indicates the entry-to-PI
  transition and `leak_lpm` records the shared nonnegative patient leak compensation
  in hundredths of L/min, limited by `MONITOR_PATIENT_LEAK_FLOW_MAX_LPM` (120 L/min).
  `VT_VOLUME_PLAN` reports the applied flow reference (hundredths of L/min),
  delivery/pause milliseconds, pressure limit (hundredths of cmH2O), and calibrated
  blower limit (hundredths of command units). Zero pause does not prevent a
  pressure-limited delivery tail; check these limits when volume remains low.
  `VT_VOLUME_FEEDBACK` reports the active plan sequence, user target, EMA VTI,
  correction and internal delivery target (volume fields in hundredths of mL).
  Waveform fields `flow_ref_lpm`, `flow_measurement_lpm`, `flow_effort`, and
  `flow_blower_ff` expose the VAC patient-side reference, feedback, PID output,
  and calibrated feedforward at a scale of 100 for direct tracking checks.
  Feedback uses proximal VTI without subtracting downstream leak. EMA alpha is 0.5 for both VTI and its applied correction. The outer loop removes
  the lag of already-applied corrections before updating with startup gain 0.8 then gain 1.0, a 0.5%
  deadband, 25% step and +/-30% total bound.
  Steady metrics use the last 400 ms of a 1-second pause: flow standard deviation,
  peak-to-peak amplitude, mean target error and error RMS. Check `steady_settled`
  before interpreting them. Raw/hysteretic crossings remain auxiliary metrics;
  they are not the acceptance criterion. Tail metrics exclude the first 120 ms.
  Connect only to a bench/test lung. The collector stops ventilation on completion
  or error; create an empty `stop` file in the output directory to request a stop.
  Missing samples invalidate the capture. `metadata.json` records controller
  tunings and source/firmware hashes alongside raw logs, CSV and summary JSON.
- `device_tool.py`: command line entry point.
- `test_vti_compensation.py`: real scheduler/phase/monitor/flow-controller host regression.
  Also covers CPAP/PSV pressure/flow triggers, flow cycling, maximum inspiration,
  apnea backup/recovery, plus PSV-ST timed cycles and invalid settings.
  Real expiration control verifies three consecutive PSV breaths for pressure and
  flow triggering, including rearming after the first breath without forced capture.
  Run `py -3 user/develop/test_vti_compensation.py`. Tests first-sample EMA initialization,
  next-breath application, duplicate/stale rejection, volume-to-flow conversion, bounds,
  convergence under a scripted 80 mL delivery loss, invalid/limited breaths and resets.
  Also checks VAC flow/pressure efforts through the real trigger engine, scheduler
  and phase controller (including negative-flow efforts), minimum expiration, volume-breath selection and the next
  mandatory time trigger with both compensation configurations.
  This is a software model, not a test-lung validation of the selected tuning.
- `test_monitor_leak.py`: production monitor and pause-controller integration test.
  Run `py -3 user/develop/test_monitor_leak.py` with native GCC/Clang. Covers known
  leakage, positive expiratory proximal flow, signed diagnostics, invalid samples,
  stop/restart, mid-breath re-zeroing, incomplete cycles and compensation limits.
  Also covers dynamic PEEP sliding five-point pressure windows, PAC completed-expiration display, PSV/ST stable-window display during trigger waits, inspiration hold, short windows and invalid pressure, PEEP alarm
  registration, inspiration-only triggering, strict thresholds and 200 ms recovery.
- `device_tool_config.json`: per-computer tool paths and target settings.
- `test_flow_pause.py`: also verifies PAC pressure targets ignore alarm-high changes while PSV/ST retain pressure caps. Native host regression for VAC pause entry and zero-flow
  control, plus VAC feedforward formula, compliance boundaries and pressure limits;
  uses the production controller and PID with sensor stubs. Run
  `py -3 user/develop/test_flow_pause.py`; native GCC/Clang is required (`CC`
  overrides discovery). Temporary host builds do not access the board or replace
  Device Tool firmware builds. Scripted inputs verify command behavior, not
  pneumatic stability or tuning.
- `vent_test_gui.py`: graphical 25-group PEEP/Delta-P test collector. It updates
  the scheme every 10 seconds while polling the incremental `vt status` data
  every 250 ms for one continuous 250-second run, then exports one CSV file.

`vt peep` prints a compact, read-only PEEP snapshot through Device Tool RTT: time,
plan sequence, mode, phase, capture readiness, patient/dynamic/display pressure
(in hundredths of cmH2O), and independent display validity.

## Computer Matching

On startup the tool detects:

- operating system: `mac`, `windows`, or `linux`
- hostname values from the local machine

It compares those values with each entry under `computers` in the config file.
If automatic matching is not enough, pass the profile name manually:

```bash
py -3 user/develop/device_tool.py --computer computer2 info
```

## Commands

Run from the repository root:

```bash
py -3 user/develop/quick_deploy.py deploy
py -3 user/develop/device_tool.py info
py -3 user/develop/quick_deploy.py build
py -3 user/develop/quick_deploy.py flash
py -3 user/develop/quick_deploy.py reset
py -3 user/develop/quick_deploy.py rtt
py -3 user/develop/vent_test_gui.py
```

Command behavior:

- `info`: prints the detected computer information and selected profile.
- `deploy`: writes the VS Code tasks, status-bar commands, and extension recommendation.
- `build`: builds the firmware with CMake, Ninja, and Arm GNU Toolchain. CMake
  configure runs only when its cache is missing or the configured tools and
  build settings change; Ninja still regenerates automatically when project
  CMake files change.
- `flash`: downloads the configured firmware image through J-Link.
- `reset`: resets the target through J-Link and lets it run.
- `rtt`: stops existing J-Link RTT/GDB server processes, starts this tool's RTT
  server, then prints RTT output from the target.
  The interactive entry resolves `_SEGGER_RTT` from the ELF beside the configured
  flash image and supplies it during the Telnet handshake. Build and flash matching
  firmware first; `DEVICE_TOOL_RTT_ADDRESS` overrides the address when the board
  runs a different image. Without an ELF/toolchain it falls back to J-Link scanning.

## PEEP / Delta-P Test GUI

Run `py -3 user/develop/vent_test_gui.py` from the repository root. The GUI
tests PEEP values 5, 10, 15, 20, and 25 against Delta-P values 10, 15, 20, 25,
and 30. It automatically sends `vt set`, starts PAC ventilation, polls `vt status`
every 250 ms, and changes to the next scheme on each absolute 10-second boundary.
The entire 25-group collection uses one continuous 250-second time line.
The GUI owns the J-Link RTT session while a collection is active and closes any
existing J-Link RTT/GDB client before it connects.
After the RTT TCP port opens, the collector performs a `help` handshake and
waits for the target console before sending any ventilation command.

Use **Start Collection** to begin, **Stop Collection** to keep all rows received
so far, and **Export CSV** after completion. The exported rows include the test
index, PEEP, Delta-P, target pressure, sample index, group-relative time, and
all waveform columns reported by `vt status`. The exporter restores the
fixed-point RTT fields to the original `gMonitorWaveformData` floating-point
values and uses the complete structure member names in the CSV header.
Sequence gaps, timestamp discontinuities, and firmware-reported dropped samples
are shown as warnings without aborting collection; all received rows remain
available for CSV export.

Use **View CSV Waveforms** to open an exported file. The viewer switches between
the 25 test groups, lets each waveform variable be enabled independently, and
overlays all selected variables in one plot. Each variable independently scales
its own Y range to the shared plot height, so signals with different units and
magnitudes remain visible. Move the mouse across the plot to inspect the nearest
curve's original sample time, value, and adaptive range.

## Config Notes

Each computer profile contains:

- `os`: expected host OS.
- `hostnames`: hostnames that identify this computer.
- `build`: compiler/build tool paths and build settings.
- `flash_image`: firmware image to download.
- `jlink`: J-Link executable paths, target device, SWD/JTAG speed, optional
  serial number, and RTT/debug ports.

Add more computers by copying `computer1` or `computer2` and changing the paths
and hostnames.

PSV-S/T regression: `py -3 user/develop/test_vti_compensation.py` exercises
startup and repeated 60000/rate deadlines, mandatory Ti, pressure/flow patient
recovery, same-tick patient priority, flow cycling, maximum spontaneous Ti,
tick wrap, stop and invalid settings using production scheduler/phase/engines.
`test_protocol.py` checks ST rate/Ti/max-Ti mapping (0x14/0x17/0x18) and isolation
from CPAP/PSV apnea timing (0x16/0x19). These tests do not operate hardware.
Use `vt psvst` to start with the selected local/host settings, `vt status` for
`VT_PSVST_SETTINGS` and the active breath, and `vt stop` to stop.

SIMV regressions use the existing `test_vti_compensation.py` harness with the real
scheduler, phase, cycle and apnea modules: adult/pediatric/neonatal windows,
window boundaries, mandatory deadlines despite spontaneous breaths, short
expiration, tick wrap, apnea entry/recovery/disable, settings validation and
volume-feedback isolation. `test_trigger.py` covers pressure/flow/off for both
SIMV modes; `test_protocol.py` covers SIMV rate, tidal volume and apnea switches.
Run with `py -3 user/develop/<script>`. These are host tests, not bench validation.

SIMV apnea selection additionally tests both pressure and volume backup in each
SIMV mode, selected-target validation, ignored inactive targets, invalid enum
values and host selection values 0/1/2 (off/pressure/volume).
