# Moore Machine Patterns & Best Practices

Advanced patterns and quick reference for experienced Moore machine developers.

## Quick Rules

### Do This
- Define finite state spaces with enums + validation
- Keep transition functions pure (no I/O, no side effects)
- Generate effects in output functions, execute them separately
- Use meaningful names and document state diagrams
- Validate state invariants with assertions

### Don't Do This  
- Put I/O operations in transition functions
- Put I/O operations in output functions (generate effects instead)
- Use unbounded state spaces (counters without limits)
- Make non-deterministic transitions
- Mix business logic with effect execution

## Advanced Patterns

### 1. Hierarchical State Machines

Break complex systems into subsystems with their own state machines:

```cpp
enum LEDMode { LED_OFF, LED_ON, LED_BLINK };
enum WiFiMode { WIFI_DISCONNECTED, WIFI_CONNECTING, WIFI_CONNECTED };

struct SystemState {
  LEDMode ledMode;
  WiFiMode wifiMode;
  bool systemEnabled;
};

// Separate transition functions for each subsystem
LEDMode updateLED(LEDMode current, const Input& input) {
  if (!input.isLEDInput()) return current;
  // ... LED-specific logic
}

WiFiMode updateWiFi(WiFiMode current, const Input& input) {
  if (!input.isWiFiInput()) return current;
  // ... WiFi-specific logic
}

SystemState transitionFunction(const SystemState& state, const Input& input) {
  SystemState newState = state;
  
  // Update subsystems
  newState.ledMode = updateLED(state.ledMode, input);
  newState.wifiMode = updateWiFi(state.wifiMode, input);
  
  // System-level transitions
  if (input.type == INPUT_SYSTEM_SHUTDOWN) {
    newState.systemEnabled = false;
    newState.ledMode = LED_OFF;
    newState.wifiMode = WIFI_DISCONNECTED;
  }
  
  return newState;
}
```

### 2. Timeout Handling

Use state entry times for automatic transitions:

```cpp
struct AppState {
  SystemMode mode;
  unsigned long stateEntryTime;
  
  // State-specific timeouts
  unsigned long getTimeout() const {
    switch (mode) {
      case MODE_CONNECTING: return 30000;  // 30 second timeout
      case MODE_WORKING: return 10000;     // 10 second timeout
      default: return 0;                   // No timeout
    }
  }
  
  bool hasTimedOut() const {
    unsigned long timeout = getTimeout();
    return timeout > 0 && (millis() - stateEntryTime > timeout);
  }
};

AppState transitionFunction(const AppState& state, const Input& input) {
  AppState newState = state;
  
  // Check for timeout on every tick
  if (input.type == INPUT_TICK && state.hasTimedOut()) {
    newState.mode = MODE_ERROR;
    newState.stateEntryTime = millis();
    return newState;
  }
  
  // Update state entry time on transitions
  if (newState.mode != state.mode) {
    newState.stateEntryTime = millis();
  }
  
  return newState;
}
```

### 3. Error Recovery with Retry Logic

Design robust error handling with bounded retries:

```cpp
struct AppState {
  SystemMode mode;
  int errorCode;
  int retryCount;
  unsigned long lastRetryTime;
  
  static const int MAX_RETRIES = 3;
  static const unsigned long RETRY_DELAY = 5000;  // 5 seconds
};

AppState transitionFunction(const AppState& state, const Input& input) {
  AppState newState = state;
  
  if (input.type == INPUT_ERROR_OCCURRED) {
    newState.mode = MODE_ERROR;
    newState.errorCode = input.errorCode;
    
    // Increment retry count
    if (state.mode != MODE_ERROR) {
      newState.retryCount = 1;
    } else {
      newState.retryCount = state.retryCount + 1;
    }
    
    newState.lastRetryTime = millis();
  }
  
  // Auto-retry logic
  if (state.mode == MODE_ERROR && input.type == INPUT_TICK) {
    if (state.retryCount < AppState::MAX_RETRIES) {
      unsigned long elapsed = millis() - state.lastRetryTime;
      if (elapsed > AppState::RETRY_DELAY) {
        newState.mode = MODE_IDLE;  // Retry
        newState.errorCode = 0;
      }
    } else {
      // Max retries exceeded - give up
      newState.mode = MODE_SHUTDOWN;
    }
  }
  
  return newState;
}
```

### 4. Input Validation at System Boundaries

Sanitize inputs before they enter the state machine:

```cpp
Input readEnvironment() {
  // Debounced button reading
  static bool lastButtonState = HIGH;
  static unsigned long lastDebounceTime = 0;
  const unsigned long DEBOUNCE_DELAY = 50;
  
  bool buttonState = digitalRead(BUTTON_PIN);
  if (buttonState != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (buttonState == LOW && lastButtonState == HIGH) {
      lastButtonState = buttonState;
      return Input::buttonPress(BUTTON_PIN);
    }
  }
  lastButtonState = buttonState;
  
  // Sensor validation
  int sensorValue = analogRead(SENSOR_PIN);
  if (sensorValue < 0 || sensorValue > 1023) {
    return Input::errorOccurred(-1);  // Invalid sensor reading
  }
  
  // Command parsing with validation
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    
    if (command == "shutdown") {
      return Input::shutdownRequested();
    } else if (command.startsWith("set ")) {
      int value = command.substring(4).toInt();
      if (value >= 0 && value <= 100) {  // Validate range
        return Input::setValue(value);
      } else {
        Serial.println("Error: Value must be 0-100");
        return Input::none();
      }
    }
  }
  
  return Input::none();
}
```

## Performance Optimization

### 1. Minimize State Copying

```cpp
// Good - minimal state
struct AppState {
  SystemMode mode;
  uint16_t counter;
  uint8_t flags;
};

// Bad - large state that's expensive to copy
struct BadState {
  char largeBuffer[1024];
  float matrix[100][100];
  String dynamicString;  // Heap allocation
};
```

### 2. Optimize Hot Paths

```cpp
AppState transitionFunction(const AppState& state, const Input& input) {
  // Fast path for common inputs
  if (input.type == INPUT_TICK) {
    AppState newState = state;
    // ... minimal tick processing
    return newState;
  }
  
  // Full processing for other inputs
  return fullTransitionFunction(state, input);
}
```

### 3. Memory Management for Embedded

```cpp
// Use stack allocation, avoid heap
struct AppState {
  char buffer[32];                    // Fixed size, stack allocated
  StaticJsonDocument<256> config;     // Fixed size JSON
  uint8_t data[16];                   // Known bounds
};

// Avoid in embedded systems
struct BadState {
  String dynamicString;               // Heap allocated
  std::vector<int> list;              // Dynamic size
  char* pointer;                      // Manual memory management
};
```

## Common Mistakes

### 1. Impure Transition Functions

```cpp
// Bad - side effects in transition function
AppState badTransition(const AppState& state, const Input& input) {
  if (input.type == INPUT_BUTTON_PRESS) {
    digitalWrite(LED_PIN, HIGH);      // Side effect!
    Serial.println("Button pressed"); // Side effect!
  }
  return state;
}

// Good - pure transition function
AppState goodTransition(const AppState& state, const Input& input) {
  AppState newState = state;
  if (input.type == INPUT_BUTTON_PRESS) {
    newState.mode = MODE_WORKING;     // Only state changes
  }
  return newState;
}

// Good - pure output function
Effect outputFunction(const AppState& state) {
  switch (state.mode) {
    case MODE_IDLE:
      return Effect::ledOff();
    case MODE_WORKING:
      return Effect::ledOn();
  }
  return Effect::none();
}

// Good - effect execution (separate from Moore machine)
void executeEffect(const Effect& effect) {
  switch (effect.type) {
    case EFFECT_LED_ON:
      digitalWrite(LED_PIN, HIGH);
      break;
    case EFFECT_LED_OFF:
      digitalWrite(LED_PIN, LOW);
      break;
  }
}
```

### 2. Non-Deterministic Behavior

```cpp
// Bad - non-deterministic
AppState badTransition(const AppState& state, const Input& input) {
  AppState newState = state;
  if (input.type == INPUT_BUTTON_PRESS) {
    // Random behavior!
    if (random(2) == 0) {
      newState.mode = MODE_WORKING;
    } else {
      newState.mode = MODE_ERROR;
    }
  }
  return newState;
}

// Good - deterministic
AppState goodTransition(const AppState& state, const Input& input) {
  AppState newState = state;
  if (input.type == INPUT_BUTTON_PRESS) {
    newState.mode = MODE_WORKING;  // Always same result
  }
  return newState;
}
```

### 3. Unbounded State Growth

```cpp
// Bad - unbounded counter
struct BadState {
  int counter;  // Can grow forever
};

// Good - bounded state
struct GoodState {
  uint8_t counter;  // 0-255, bounded
  static const uint8_t MAX_COUNT = 100;
  
  void incrementCounter() {
    if (counter < MAX_COUNT) {
      counter++;
    }
  }
};
```

## Testing Strategies

### Unit Test Transition Functions

```cpp
void testBasicTransitions() {
  AppState idle = { MODE_IDLE, 0, 0 };
  
  // Test valid transition
  Input buttonPress = Input::buttonPress(1);
  AppState working = transitionFunction(idle, buttonPress);
  assert(working.mode == MODE_WORKING);
  
  // Test invalid input ignored
  Input invalidInput = Input::sensorReading(100);
  AppState unchanged = transitionFunction(idle, invalidInput);
  assert(unchanged.mode == MODE_IDLE);
  
  Serial.println("✓ Basic transitions work");
}
```

### State Invariant Validation

```cpp
AppState transitionFunction(const AppState& state, const Input& input) {
  // Validate input state
  assert(state.mode < MODE_COUNT);
  assert(state.counter <= MAX_COUNTER);
  
  AppState newState = /* ... transition logic ... */;
  
  // Validate output state
  assert(newState.mode < MODE_COUNT);
  assert(newState.counter <= MAX_COUNTER);
  
  return newState;
}
```

These patterns help you build robust, maintainable Moore machines while avoiding common pitfalls in embedded development.