# Device Tool

`device_tool.py` is a local development helper for this firmware project. It reads
`device_tool_config.json`, matches the current computer by OS and hostname, then
uses the matched profile to build, flash, reset, and read RTT logs from the
target board.

## Files

- `device_tool.py`: command line entry point.
- `device_tool_config.json`: per-computer tool paths and target settings.
- `vent_test_gui.py`: graphical 25-group PEEP/Delta-P test collector. It updates
  the scheme every 10 seconds while polling the incremental `vt status` data
  every 250 ms for one continuous 250-second run, then exports one CSV file.

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
all waveform columns reported by `vt status`. The exporter restores the 17
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
