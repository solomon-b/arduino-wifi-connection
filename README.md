# MooreArduino - Moore Machine Library for Arduino

A complete Moore finite state machine implementation for Arduino development. Provides predictable, mathematically-grounded state management for embedded systems.

## What is a Moore Machine?

A Moore machine is a finite automaton where outputs depend only on the current state, not on the inputs. It's defined as M = (Q, Σ, δ, λ, q₀) where:

- **Q** is a finite set of states
- **Σ** is a finite set of input symbols  
- **δ: Q × Σ → Q** is the state transition function
- **λ: Q → Γ** is the output function that generates effects
- **Γ** is a finite set of output symbols (effects)
- **q₀** is the initial state

*Note: If you're familiar with Redux, a Redux store is precisely a Moore machine where actions are input symbols (Σ), reducers are transition functions (δ), and side effects are output functions (λ).*

## Why Moore Machines for Arduino?

Moore machines provide several advantages over traditional Arduino programming:

- **Predictable behavior**: Outputs depend only on current state
- **Deterministic transitions**: Same input + state = same result
- **Easy testing**: Pure functions with no hidden dependencies
- **Mathematical foundation**: Formal model with proven properties
- **Clean separation**: Logic (δ) vs I/O (λ) vs state (Q)
- **Scalable**: Complex systems stay manageable

## Library Components

### Core State Machine
- **MooreMachine<State, Input, Output>**: Template-based finite state machine
- **Pure transition functions**: δ(q, σ) → q' with no side effects
- **Output functions**: λ(q) → effects with I/O isolation
- **Output execution**: All I/O operations handled separately
- **State observers**: Reactive patterns for UI updates

### Utility Classes
- **Timer**: Non-blocking timer with start/stop/expired methods
- **Button**: Debounced button input with configurable delay
- **AsyncOp**: Async operation tracking with timeout management

## Quick Start

```cpp
#include <MooreArduino.h>
using namespace MooreArduino;

// Define your state space Q
enum SystemState { IDLE, WORKING, ERROR };

struct AppState {
  SystemState mode;
  int counter;
  
  AppState() : mode(IDLE), counter(0) {}
};

// Define your input alphabet Σ  
enum InputType { INPUT_BUTTON, INPUT_TICK, INPUT_RESET };

struct Input {
  InputType type;
  
  static Input button() { Input i; i.type = INPUT_BUTTON; return i; }
  static Input tick() { Input i; i.type = INPUT_TICK; return i; }
};

// Define your effect alphabet Γ
enum OutputType { EFFECT_NONE, EFFECT_LED_ON, EFFECT_LED_OFF };

struct Output {
  OutputType type;
  
  static Output none() { Output e; e.type = EFFECT_NONE; return e; }
  static Output ledOn() { Output e; e.type = EFFECT_LED_ON; return e; }
  static Output ledOff() { Output e; e.type = EFFECT_LED_OFF; return e; }
};

// Pure transition function δ: Q × Σ → Q
AppState transitionFunction(const AppState& state, const Input& input) {
  AppState newState = state;
  
  switch (state.mode) {
    case IDLE:
      if (input.type == INPUT_BUTTON) {
        newState.mode = WORKING;
        newState.counter = 0;
      }
      break;
      
    case WORKING:
      if (input.type == INPUT_TICK) {
        newState.counter++;
        if (newState.counter >= 10) {
          newState.mode = IDLE;
        }
      }
      break;
  }
  
  return newState;
}

// Output function λ: Q → Γ (generates effects)
Output outputFunction(const AppState& state) {
  // Moore property: outputs depend only on current state
  switch (state.mode) {
    case IDLE:
      return Output::ledOff();
    case WORKING:
      return Output::ledOn();
    case ERROR:
      return Output::none();
  }
  return Output::none();
}

// Execute effects (handle I/O)
void executeEffect(const Output& effect) {
  switch (effect.type) {
    case EFFECT_LED_ON:
      digitalWrite(LED_PIN, HIGH);
      break;
    case EFFECT_LED_OFF:
      digitalWrite(LED_PIN, LOW);
      break;
    case EFFECT_NONE:
    default:
      break;
  }
}

// Create and use Moore machine
MooreMachine<AppState, Input, Output> machine(transitionFunction, AppState());

void setup() {
  machine.setOutputFunction(outputFunction);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  // 1. Get current effect from Moore machine λ: Q → Γ
  Output output = machine.getCurrentOutput();
  
  // 2. Execute effect (handle I/O)
  executeEffect(output);
  
  // 3. Process inputs
  Input input = readEnvironment();  // Your input logic here
  if (input.type != INPUT_NONE) {
    machine.step(input);
  }
  delay(10);
}
```

## Examples

The library includes several complete examples demonstrating different patterns:

### 1. WiFi Connection Manager (`examples/WiFiManager/`)
Complete WiFi credential management with persistent storage:
- **State Space**: {INITIALIZING, CONNECTING, CONNECTED, DISCONNECTED, ENTERING_CREDENTIALS}
- **Features**: KVStore persistence, LED status, serial interface
- **Hardware**: Arduino Giga R1 WiFi

### 2. Smart LED Controller (`examples/`) 
Multi-mode LED controller with hierarchical state machines:
- **Modes**: Manual, WiFi, Auto (light sensor)
- **Patterns**: Solid, blink, fade with brightness control
- **Demonstrates**: Mode switching, sensor integration

## Installation

### Method 1: Arduino Library Manager
1. Open Arduino IDE
2. Go to Sketch → Include Library → Manage Libraries
3. Search for "MooreArduino"
4. Click Install

### Method 2: Manual Installation
1. Download or clone this repository
2. Copy `MooreArduino/` folder to your Arduino libraries directory
3. Restart Arduino IDE

### Method 3: Development Setup (Nix)
For development with the included examples:

```bash
# Clone repository
git clone https://github.com/user/arduino-wifi-connection.git
cd arduino-wifi-connection

# Build WiFi example
nix run '.#build'

# Upload to board
nix run '.#load'

# Monitor serial output  
nix run '.#monitor'
```

## Arduino UDEV Setup

For Linux users, ensure proper USB permissions:

```bash
# Add to /etc/udev/rules.d/99-arduino.rules
SUBSYSTEMS=="usb", ATTRS{idVendor}=="2e8a", MODE:="0666"
SUBSYSTEMS=="usb", ATTRS{idVendor}=="2341", MODE:="0666"
SUBSYSTEMS=="usb", ATTRS{idVendor}=="1fc9", MODE:="0666"
SUBSYSTEMS=="usb", ATTRS{idVendor}=="0525", MODE:="0666"
```

## Documentation

- **[TUTORIAL.md](TUTORIAL.md)**: Complete Moore machine programming guide
- **[PATTERNS.md](PATTERNS.md)**: Best practices and common patterns  
- **[EXAMPLES.md](EXAMPLES.md)**: Detailed example walkthroughs

## API Reference

### MooreMachine<State, Input, Output>

```cpp
// Constructor
MooreMachine(TransitionFunction δ, const State& q₀)

// Process input through state machine
void step(const Input& input)

// Get current state (read-only)
const State& getState() const

// Get current effect from output function λ: Q → Γ
Output getCurrentOutput() const

// Set output function λ
void setOutputFunction(OutputFunction λ)

// Add state change observer
bool addStateObserver(StateObserver observer)
```

### Utility Classes

```cpp
// Timer - non-blocking delays
Timer timer(1000);  // 1 second interval
timer.start();
if (timer.expired()) { /* do something */ }

// Button - debounced input
Button btn(2);  // Pin 2 with pull-up
if (btn.wasPressed()) { /* handle press */ }

// AsyncOp - timeout tracking  
AsyncOp op;
op.start(5000);  // 5 second timeout
if (op.timedOut()) { /* handle timeout */ }
```

## Design Philosophy

This library implements Moore machines directly rather than hiding them behind framework abstractions. Key principles:

1. **Mathematical Foundation**: Based on formal automata theory
2. **Pure Functions**: Transition functions have no side effects
3. **I/O Isolation**: All I/O happens in effect execution functions  
4. **Type Safety**: Template-based for compile-time correctness
5. **Embedded-Friendly**: Minimal memory usage, no dynamic allocation

## Performance

- **Minimal Memory**: Template instantiation only includes used features
- **No Dynamic Allocation**: All data structures are stack-allocated  
- **Fast Execution**: Pure functions optimize well
- **Predictable Timing**: No hidden operations or callbacks

## Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality  
4. Ensure examples still build
5. Submit a pull request

## License

MIT License - see LICENSE file for details.
