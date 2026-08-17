# Device Tool

`device_tool.py` is a local development helper for this firmware project. It reads
`device_tool_config.json`, matches the current computer by OS and hostname, then
uses the matched profile to build, flash, reset, and read RTT logs from the
target board.

## Files

- `device_tool.py`: command line entry point.
- `device_tool_config.json`: per-computer tool paths and target settings.

## Computer Matching

On startup the tool detects:

- operating system: `mac`, `windows`, or `linux`
- hostname values from the local machine

It compares those values with each entry under `computers` in the config file.
If automatic matching is not enough, pass the profile name manually:

```bash
python3 rep/scripts/develop/device_tool.py --computer computer1 info
```

## Commands

Run from the repository root:

```bash
python3 rep/scripts/develop/device_tool.py info
python3 rep/scripts/develop/device_tool.py build
python3 rep/scripts/develop/device_tool.py flash
python3 rep/scripts/develop/device_tool.py reset
python3 rep/scripts/develop/device_tool.py rtt
```

Command behavior:

- `info`: prints the detected computer information and selected profile.
- `build`: builds the firmware with the configured compiler. The current macOS
  profile uses CMake, Ninja, and Arm GNU Toolchain.
- `flash`: downloads the configured firmware image through J-Link.
- `reset`: resets the target through J-Link and lets it run.
- `rtt`: stops existing J-Link RTT/GDB server processes, starts this tool's RTT
  server, then prints RTT output from the target.

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
