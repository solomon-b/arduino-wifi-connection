# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MooreArduino is a Moore finite state machine library for Arduino development. A Moore machine is a finite automaton where outputs depend only on the current state, providing predictable and mathematically-grounded state management for embedded systems.

The library provides:
- Template-based Moore machine implementation (`MooreMachine<State, Input>`)
- Utility classes for common embedded patterns (Timer, Button, AsyncOp)
- Complete examples demonstrating Moore machine patterns
- Mathematical foundation with formal δ (transition) and λ (output) functions

**Mathematical Model**: M = (Q, Σ, δ, λ, q₀) where:
- Q is the finite state space
- Σ is the input alphabet  
- δ: Q × Σ → Q is the pure transition function
- λ: Q → Γ is the output function handling I/O
- q₀ is the initial state

**Note**: Redux is actually a specific implementation of Moore machines. This library provides the underlying computational model directly.

## Development Commands

Build the WiFi example:
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

Example hardware setup (for WiFi manager example):
- Arduino Giga R1 board (FQBN: arduino:mbed_giga:giga)
- Power LED: Digital Pin 2 (always on when powered)
- WiFi LED: Digital Pin 3 (on when connected to WiFi)
- Serial connection: /dev/ttyACM0 at 115200 baud

Other examples may use different pin configurations - see individual example documentation.

## Repository Structure

```
MooreArduino/                    # Arduino library
├── src/
│   ├── MooreMachine.h          # Core Moore machine template
│   ├── Timer.h                 # Non-blocking timer utility
│   ├── Button.h                # Debounced button input
│   ├── AsyncOp.h               # Async operation tracking
│   └── MooreArduino.h          # Main library header
├── library.properties          # Arduino library metadata
└── keywords.txt                # Arduino IDE syntax highlighting

examples/
├── WiFiManager/                # WiFi credential management
│   └── WiFiManager.ino         # Complete WiFi state machine
└── simple-led/                 # Basic LED controller
    └── simple-led.ino          # Button-controlled LED modes

Documentation:
├── README.md                   # Library overview and API reference
├── TUTORIAL.md                 # Moore machine programming guide
├── PATTERNS.md                 # Best practices and design patterns
└── EXAMPLES.md                 # Detailed example walkthroughs
```

## Key Concepts

### Pure Transition Functions
All business logic goes in pure transition functions δ that take (state, input) and return new state with no side effects.

### I/O Isolation
All I/O operations happen in output functions λ that depend only on current state (Moore property).

### Template-Based Design
The library uses C++ templates for type safety and zero-cost abstractions suitable for embedded systems.

### Mathematical Foundation
Based on formal automata theory, providing provable properties and predictable behavior.

## Development Guidelines

1. **Define finite state spaces**: Use enums for states and inputs
2. **Keep transitions pure**: No I/O in transition functions
3. **Handle I/O in output functions**: All side effects based on current state
4. **Use utility classes**: Timer, Button, AsyncOp for common patterns
5. **Test transition functions**: Pure functions are easy to unit test
6. **Document state machines**: Clear Q, Σ, δ, λ separation

## Example Usage

```cpp
#include <MooreArduino.h>
using namespace MooreArduino;

// Define state space Q and input alphabet Σ
enum SystemState { IDLE, WORKING };
enum InputType { INPUT_BUTTON, INPUT_TICK };

// Pure transition function δ: Q × Σ → Q  
AppState transition(const AppState& state, const Input& input) { ... }

// Create Moore machine
MooreMachine<AppState, Input> machine(transition, AppState());
```