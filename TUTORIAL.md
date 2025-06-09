# Moore Machine Programming for Arduino

## What This Is

This tutorial shows you how to write Arduino code using Moore finite state machines - a mathematical model that makes embedded systems predictable and maintainable. Instead of global variables scattered everywhere and spaghetti code, you model your system as a formal state machine.

A Moore machine is defined as M = (Q, Σ, δ, λ, q₀) where outputs depend only on the current state, making behavior completely predictable.

## What This Is NOT

This is **not** Functional Reactive Programming (FRP). Real FRP has continuous streams and behaviors. This is also **not** Redux (though Redux is actually a Moore machine implementation). This is the underlying computational model that makes those patterns work.

## Why Moore Machines?

Arduino code usually starts simple but gets messy fast:

```cpp
// This gets ugly quick
int led_state = 0;
bool wifi_connected = false;
unsigned long last_blink = 0;
char ssid[32];
bool connecting = false;
// ... and it keeps growing
```

With Moore machines, you get:
- **Predictable behavior**: Output depends only on current state
- **Deterministic transitions**: Same input + state = same result  
- **Mathematical foundation**: Formal model with proven properties
- **Easy testing**: Pure functions with no hidden dependencies
- **Clear separation**: Logic (δ) vs I/O (λ) vs state (Q)

## The Mathematical Model

A Moore machine processes inputs through deterministic state transitions:

```
Input → δ(currentState, input) → newState → λ(newState) → outputs
```

Key properties:
- **δ (delta)**: Transition function - pure, no side effects
- **λ (lambda)**: Output function - handles I/O based on state
- **Deterministic**: Same input + state always produces same result
- **Moore property**: Outputs depend only on state, not input

## Core Concepts

### 1. Define Your State Space Q

Instead of scattered variables, define a finite set of states:

```cpp
// Bad - state everywhere
int led_mode = 0;
bool wifi_connecting = false;
bool has_credentials = true;

// Good - finite state space
enum SystemState {
  STATE_IDLE,
  STATE_CONNECTING_WIFI,
  STATE_CONNECTED,
  STATE_ERROR
};

struct AppState {
  SystemState mode;
  Credentials credentials;
  unsigned long lastUpdate;
};
```

### 2. Define Your Input Alphabet Σ

Enumerate all possible inputs to your system:

```cpp
enum InputType {
  INPUT_BUTTON_PRESSED,
  INPUT_WIFI_CONNECTED,
  INPUT_WIFI_FAILED,
  INPUT_TIMER_EXPIRED,
  INPUT_USER_COMMAND
};

struct Input {
  InputType type;
  // Additional data for specific inputs
  int buttonPin;
  char command[32];
};
```

### 3. Write Pure Transition Function δ

The transition function must be pure - no I/O, no side effects:

```cpp
// δ: Q × Σ → Q
AppState transitionFunction(const AppState& state, const Input& input) {
  AppState newState = state;  // Copy current state
  newState.lastUpdate = millis();
  
  switch (state.mode) {
    case STATE_IDLE:
      if (input.type == INPUT_BUTTON_PRESSED) {
        newState.mode = STATE_CONNECTING_WIFI;
      }
      break;
      
    case STATE_CONNECTING_WIFI:
      if (input.type == INPUT_WIFI_CONNECTED) {
        newState.mode = STATE_CONNECTED;
      } else if (input.type == INPUT_WIFI_FAILED) {
        newState.mode = STATE_ERROR;
      }
      break;
      
    // ... other states
  }
  
  return newState;  // Return new state, never modify input
}
```

### 4. Handle I/O in Output Function λ

All side effects happen in the output function. 

**Note**: Our implementation differs from the pure mathematical Moore machine model for practical reasons:

**Pure Mathematical Model:**
```cpp
void outputFunction(const AppState& currentState)  // λ: Q → Γ
```
- Takes only current state
- Produces outputs in alphabet Γ (LED signals, sounds, etc.)
- No return value needed

**Our Practical Implementation:**
```cpp
Input outputFunction(const AppState& oldState, const AppState& newState)
```
- Takes both old and new state (for state entry actions)
- Returns optional follow-up Input (from Σ, not Γ)
- Handles both true Moore outputs (I/O) AND input generation

**Why the differences:**

1. **State entry actions**: We need `oldState` to detect transitions and perform actions only when entering a state (like starting WiFi connection only once, not continuously)

2. **Follow-up inputs**: Starting async operations often needs to set up timeouts or generate events - returning an Input lets us feed these back into the machine

3. **Practical I/O**: The actual I/O operations (`digitalWrite`, `Serial.print`) are the true Moore outputs (Γ), but we also need to handle the practical concerns of embedded programming

So our output function does **two jobs**:
- **Moore outputs (Γ)**: The I/O side effects based on current state  
- **Input generation (Σ)**: Optional follow-up events for async operations

```cpp
// λ: Q → Γ (generates follow-up inputs)
Input outputFunction(const AppState& oldState, const AppState& newState) {
  // Update LEDs based on current state (Moore property)
  switch (newState.mode) {
    case STATE_IDLE:
      digitalWrite(LED_PIN, LOW);
      break;
    case STATE_CONNECTING_WIFI:
      digitalWrite(LED_PIN, millis() % 500 < 250);  // Blink
      break;
    case STATE_CONNECTED:
      digitalWrite(LED_PIN, HIGH);
      break;
  }
  
  // Initiate side effects when entering new states
  if (oldState.mode != STATE_CONNECTING_WIFI && 
      newState.mode == STATE_CONNECTING_WIFI) {
    WiFi.begin(newState.credentials.ssid, newState.credentials.pass);
    return Input::wifiConnectionStarted();  // Follow-up input
  }
  
  return Input::none();  // No follow-up needed
}
```

### 5. Create and Use Moore Machine

```cpp
#include <MooreArduino.h>
using namespace MooreArduino;

// Create Moore machine instance
MooreMachine<AppState, Input> machine(transitionFunction, AppState());

void setup() {
  // Set up output function and observers
  machine.setOutputFunction(outputFunction);
  machine.addStateObserver(logStateChanges);
}

void loop() {
  // Read inputs from environment
  Input input = readEnvironment();
  
  // Process through Moore machine
  if (input.type != INPUT_NONE) {
    machine.step(input);
  }
  
  delay(10);
}
```

## State Observers

### What Are Observers?

State observers are read-only functions that get called whenever the state changes. They're useful for debugging, logging, metrics, and notifying external systems without affecting the core state machine logic.

```cpp
void logStateChanges(const AppState& oldState, const AppState& newState) {
  if (oldState.mode != newState.mode) {
    Serial.print("State transition: ");
    Serial.print(getModeString(oldState.mode));
    Serial.print(" → ");
    Serial.println(getModeString(newState.mode));
  }
}
```

### How Observers Differ from Output Functions

**Output Functions λ:**
- Handle I/O based on current state (Moore property)
- Can return follow-up inputs
- Are part of the core state machine operation
- Should perform necessary side effects

**Observers:**
- Only monitor state changes
- Cannot return inputs or affect the machine
- Are optional debugging/monitoring tools
- Called after transitions are complete

### Adding Multiple Observers

You can add multiple observers for different purposes:

```cpp
void debugObserver(const AppState& old, const AppState& new) {
  Serial.print("Debug: transition at ");
  Serial.println(millis());
}

void metricsObserver(const AppState& old, const AppState& new) {
  transitionCount++;
  lastTransitionTime = millis();
}

void apiObserver(const AppState& old, const AppState& new) {
  // Notify external system
  sendStateUpdate(new.mode);
}

void setup() {
  machine.setOutputFunction(outputFunction);
  machine.addStateObserver(debugObserver);
  machine.addStateObserver(metricsObserver);
  machine.addStateObserver(apiObserver);
}
```

### Common Observer Use Cases

- **Debugging**: Log all state transitions during development
- **Metrics**: Count transitions, measure timing, track usage
- **External APIs**: Notify servers or other devices of state changes
- **Data logging**: Record state history to storage
- **User feedback**: Show status messages or notifications
- **Testing**: Verify expected state transitions during automated tests

```cpp
// Example: Comprehensive logging observer
void fullStateLogger(const AppState& oldState, const AppState& newState) {
  if (oldState.mode != newState.mode) {
    Serial.print("[");
    Serial.print(millis());
    Serial.print("] ");
    Serial.print(getModeString(oldState.mode));
    Serial.print(" → ");
    Serial.print(getModeString(newState.mode));
    Serial.print(" (counter: ");
    Serial.print(newState.counter);
    Serial.println(")");
  }
}
```

**Key principle**: Observers are for watching and reacting to state changes, not for controlling the state machine itself.

## State Transition Design

### Plan Your States

Think carefully about your finite state space Q:

```cpp
// LED Controller example
enum LEDState {
  LED_OFF,
  LED_ON, 
  LED_BLINK_SLOW,
  LED_BLINK_FAST,
  LED_FADE
};
```

### Map Valid Transitions

Not all transitions are valid. Document your state machine:

```
LED_OFF → LED_ON (button press)
LED_ON → LED_BLINK_SLOW (button press)  
LED_BLINK_SLOW → LED_BLINK_FAST (button press)
LED_BLINK_FAST → LED_OFF (button press)
Any state → LED_OFF (timeout)
```

### Handle Invalid Inputs

Your transition function should handle unexpected inputs gracefully:

```cpp
AppState transitionFunction(const AppState& state, const Input& input) {
  AppState newState = state;
  
  switch (state.mode) {
    case LED_OFF:
      if (input.type == INPUT_BUTTON_PRESS) {
        newState.mode = LED_ON;
      }
      // Ignore other inputs when OFF
      break;
      
    default:
      // Log unexpected state
      Serial.println("Unknown state in transition function");
      break;
  }
  
  return newState;
}
```

## Testing Your Moore Machine

Moore machines are easy to test because they're pure functions:

```cpp
void testLEDTransitions() {
  AppState state = { LED_OFF, 0 };
  
  // Test: button press should turn LED on
  Input buttonPress = Input::buttonPressed();
  AppState newState = transitionFunction(state, buttonPress);
  
  assert(newState.mode == LED_ON);
  Serial.println("✓ Button press turns LED on");
  
  // Test: invalid input should be ignored
  Input invalidInput = Input::wifiConnected();
  AppState unchanged = transitionFunction(state, invalidInput);
  
  assert(unchanged.mode == LED_OFF);
  Serial.println("✓ Invalid inputs ignored");
}
```

## Common Patterns

### Timer-Based State Changes

```cpp
struct AppState {
  SystemMode mode;
  unsigned long stateEntryTime;
  unsigned long timeout;
};

AppState transitionFunction(const AppState& state, const Input& input) {
  AppState newState = state;
  
  // Check for timeout in any state
  if (input.type == INPUT_TICK) {
    unsigned long elapsed = millis() - state.stateEntryTime;
    if (elapsed > state.timeout) {
      newState.mode = STATE_TIMEOUT;
      newState.stateEntryTime = millis();
    }
  }
  
  // Handle state entry
  if (newState.mode != state.mode) {
    newState.stateEntryTime = millis();
    newState.timeout = getTimeoutForState(newState.mode);
  }
  
  return newState;
}
```

### Multi-Device Coordination

```cpp
struct SystemState {
  LEDMode ledMode;
  WiFiMode wifiMode;
  SensorMode sensorMode;
  bool systemEnabled;
};

// Coordinate multiple subsystems
AppState transitionFunction(const AppState& state, const Input& input) {
  AppState newState = state;
  
  if (input.type == INPUT_SYSTEM_SHUTDOWN) {
    // Coordinated shutdown
    newState.ledMode = LED_OFF;
    newState.wifiMode = WIFI_DISCONNECTED;
    newState.sensorMode = SENSOR_SLEEP;
    newState.systemEnabled = false;
  }
  
  return newState;
}
```

## Debugging Tips

### Add State Logging

```cpp
void logStateChanges(const AppState& oldState, const AppState& newState) {
  if (oldState.mode != newState.mode) {
    Serial.print("State: ");
    Serial.print(getStateName(oldState.mode));
    Serial.print(" → ");
    Serial.println(getStateName(newState.mode));
  }
}
```

### Visualize State Machine

Draw your state machine on paper or use tools like:
- State transition diagrams
- State tables
- Timing diagrams

### Use Assertions

```cpp
AppState transitionFunction(const AppState& state, const Input& input) {
  // Validate state invariants
  assert(state.mode >= 0 && state.mode < STATE_COUNT);
  assert(state.lastUpdate <= millis());
  
  // ... transition logic
  
  // Validate output
  assert(newState.mode >= 0 && newState.mode < STATE_COUNT);
  return newState;
}
```

## When to Use Moore Machines

Moore machines work well for:
- **State-heavy systems**: Multiple modes, complex coordination
- **Safety-critical code**: Predictable, testable behavior
- **User interfaces**: Button handling, menu systems
- **Communication protocols**: Connection states, handshakes
- **Embedded control**: Motor control, sensor management

Moore machines might be overkill for:
- Simple linear programs
- Pure computational tasks
- Systems with minimal state

## Mathematical Properties

Moore machines have useful mathematical properties:

- **Deterministic**: δ(q, σ) always produces the same result
- **Finite**: Only finitely many states and inputs
- **Memoryless**: Only current state matters, not history
- **Composable**: Multiple machines can be combined
- **Analyzable**: Can prove properties about behavior

This mathematical foundation makes your Arduino code more reliable and easier to reason about than traditional imperative programming approaches.

## Further Reading

- Automata theory textbooks
- Formal verification techniques  
- State machine design patterns
- Real-time systems programming