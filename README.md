# wCron - Unix-Style Cron Service for Windows

wCron is a lightweight Windows service that provides Unix-style cron functionality for scheduling and executing tasks based on a cron-like configuration file. It supports time-based task scheduling, logging, and service management.

## Features

- **Cron Scheduling**: Supports standard cron syntax for minute, hour, day, month, and weekday, with extensions like `*` (every), `?` (startup time), ranges (`0-23`), steps (`1-23/4`), and lists (`0,3-6/2,9`).
- **Configuration**: Reads tasks from `<basename>.tab` (default) and additional files via `@cronfile`. Supports directives for logging (`@log`), command extensions (`@cmdext`), startup/shutdown scripts (`@startupcmd`, `@shutdowncmd`), and output directories (`@loginfodir`).
- **Task Execution**: Runs `.exe` files and scripts with mapped extensions (e.g., `.cmd` to `cmd.exe /C`). Redirects output to per-job log files.
- **Service Management**: Installs as a Windows service with automatic start, supporting stop, pause, continue, and shutdown. Executes startup/shutdown scripts if defined.
- **Logging**: Configurable log levels (0–9, default 3) to `<basename>_log.txt`. Job-specific logs in `<basename>_<cmd>_<hash>.info.txt`.
- **Console Mode**: Debug with `-console` or `-testmode` (simulates without executing). View parsed entries with `-entries`.
- **Multiple crons**: You can have multiple cron running as a separate user, just rename the executable.
  
## Installation

1. **Compile**:
   - Requires a C++ compiler with Windows SDK (e.g., Visual Studio).
   - Compile `dynarray.c`, `wCron_core.c`, `wCron_service.c`, and `main.c`.

2. **Setup**:
   - Place the compiled `wCron.exe` in a directory (e.g., `C:\wCron`).
   - Create a `<basename>.tab` file in the same directory (default: `wCron.tab`).

3. **Install Service**:
   wCron.exe -install
Installs and starts the service. The service name is derived from the executable name (e.g., `wCron`).

## Usage

### Command-Line Options
wCron.exe [-install] [-remove] [-start] [-stop] [-status] [-console] [-testmode] [-entries] [-version]
- `-install`: Install and start the service.
- `-remove`: Stop and uninstall the service.
- `-start`: Start the service.
- `-stop`: Stop the service.
- `-status`: Show service status.
- `-console`: Run in console mode for debugging.
- `-testmode`: Run in console mode without executing tasks.
- `-entries`: Display parsed cron entries.
- `-version`: Show version (4.05).

### Cron File Format
The cron file (`wCron.tab`) defines tasks with the following syntax:
Format: minute hour day month weekday command [parameters]
command.exe param1 param2
0 12 * * 1-5 script.cmd
- **Time Fields**:
  - `*`: Every unit.
  - `?`: Startup time.
  - `0-23`: Range.
  - `1-23/4`: Range with step.
  - `0,3-6/2,9`: Multiple values.
- **Directives**:
  - `@log=[0-9]`: Set log level (0=none, 9=verbose, default 3).
  - `@cronfile=<file>`: Include additional cron file.
  - `@cmdext=<ext>;<exe>[;<opts>]`: Map extension to executable (e.g., `cmdext=cmd;C:\Windows\System32\cmd.exe;/C`).
  - `@shutdowncmd=<cmd>`: Run on service shutdown.
  - `@startupcmd=<cmd>`: Run on service start (within 15 minutes of boot).
- **Logging Flags** (before command):
  - `~`: Disable logging.
  - `-`: Minimal logging.
  - `=`: Normal logging.
  - `+`: Verbose logging.


## Logging

- **Main Log**: `<basename>_log.txt` (e.g., `wCron_log.txt`) in the executable directory. Rotates daily for log level > 2.
- **Job Logs**: `<basename>_<cmd>_<hash>.info.txt` for command output, optionally in `LOGinfodir`.
- **Log Levels**:
  - 0: None.
  - 1: Minimal.
  - 2: Errors.
  - 3: Normal (default).
  - 9: Verbose.

