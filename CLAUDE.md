# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Arduino Giga R1 demo application that manages WiFi connections with credential persistence using Redux/Elm Architecture patterns. The app prompts for SSID and password via serial, stores them using KVStore, and indicates connection status with LEDs.

This project demonstrates Redux-style state management on microcontrollers with:
- Single source of truth (centralized state)
- Unidirectional data flow (Events → Actions → Reducer → State → Side Effects)
- Pure reducers for predictable state transitions
- Isolated side effects for I/O operations
- Observer pattern for reactive UI updates

**Note**: This is NOT Functional Reactive Programming (FRP) - it's Redux/Elm Architecture adapted for embedded systems.

## Development Commands

Build the project:
```bash
nix run '.#build'
```

Upload to board (compiles and uploads):
```bash
nix run '.#load'
```

Monitor serial output:
```bash
nix run '.#monitor'
```

## Hardware Configuration

- Arduino Giga R1 board (FQBN: arduino:mbed_giga:giga)
- Power LED: Digital Pin 2 (always on when powered)
- WiFi LED: Digital Pin 3 (on when connected to WiFi)
- Serial connection: /dev/ttyACM0 at 115200 baud

## Architecture

- Single-file Arduino sketch (`wifi-connection.ino`)
- Uses KVStore API for persistent credential storage
- WiFi management with connection status feedback
- Serial interface for credential input and status monitoring
- Development environment managed by Nix flake with arduino-nix