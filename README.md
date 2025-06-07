# xmkctr
`xmkctr` is a GUI usage data logger for X.Org.

This program logs each of mouse distance, mouse L/R clicks and key down counts in one minute. Mouse localion, key kinds are not logged.

# Output
The log file is simple CSV. You can change output directory by `xmkctr.txt`.

## Format
Column description row is not included.

Please refer column definitions below.

| Column index | Description |
| --- | --- |
| 0 | Logging start time <br> ISO format |
| 1 | Mouse move disctance in pixel |
| 2 | Mouse L click count |
| 3 | Mouse R click count |
| 4 | Keyboard key down count |

## File
The log filen will be renewed in one hour.

The file name format is `YYYY-mm-dd_HH.csv`.

Current written file is shown as another `*.lck` file. This file is empty and deleted by `xmkctr`.

# Installing
## Prerequests
This program checked with Ubuntu 24.04 and X.Org. Other distributions and/or Wayland are not checked.
- Git
- X.Org
- X.Org libs
  - X11/Xlib.h
  - X11/extensions/XInput2.h
- gcc
- make

### X.Org libs
Packages in `apt` are below.
- libx11-dev
- libxi-dev

## Build
1. Pull or download this repository.
1. Move to local directory and run `make`.
1. Set `xmkctr.txt` to your CSV output directory.
1. Run `xmkctr` on your desktop terminal.
1. You can stop by `SIGINT` signal.

# License
MIT