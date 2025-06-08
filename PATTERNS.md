# Common Patterns & Tips

Quick reference for Redux-style Arduino code.

## Basic Rules

### Do This
- Keep all state in one struct
- Change state only through reducers
- Handle I/O outside of reducers
- Use enums instead of magic numbers
- Name actions based on what happened, not what you want to do

### Don't Do This
- Scatter state variables everywhere
- Mix I/O with state logic
- Modify state directly
- Block the main loop with long operations
- Use meaningless names like `SET_FLAG_1`

## Common Patterns

### 1. State Setup

Always provide sensible defaults:

```cpp
struct AppState {
  bool isConnected;
  int retryCount;
  unsigned long lastTry;
  
  // Set reasonable defaults
  AppState() : isConnected(false), retryCount(0), lastTry(0) {}
};
```

### 2. Action Factories

Use static methods to create actions:

```cpp
struct Action {
  ActionType type;
  int value;
  
  static Action connect() {
    Action a;
    a.type = CONNECT;
    return a;
  }
  
  static Action setBrightness(int brightness) {
    Action a;
    a.type = SET_BRIGHTNESS;
    a.value = constrain(brightness, 0, 255);  // validate here
    return a;
  }
};
```

### 3. Timers

Simple timer pattern:

```cpp
struct Timer {
  unsigned long interval;
  unsigned long lastTrigger;
  bool running;
  
  Timer(unsigned long ms) : interval(ms), lastTrigger(0), running(false) {}
  
  void start() {
    lastTrigger = millis();
    running = true;
  }
  
  bool expired() {
    return running && (millis() - lastTrigger >= interval);
  }
  
  void stop() { running = false; }
  
  void restart() { start(); }
};

// Use in your state
struct AppState {
  Timer heartbeat;
  Timer reconnect;
  
  AppState() : heartbeat(1000), reconnect(30000) {
    heartbeat.start();
  }
};

// In your reducer
case ACTION_TICK:
  if (state.heartbeat.expired()) {
    newState.heartbeat.restart();
    // do heartbeat stuff
  }
  break;
```

### 4. Reading Events

Handle different priority levels:

```cpp
Action readEvents(const AppState& state) {
  // High priority: safety/emergency
  if (digitalRead(EMERGENCY_STOP_PIN) == LOW) {
    return Action::emergencyStop();
  }
  
  // Medium priority: user input
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);  // simple debounce
    return Action::buttonPressed();
  }
  
  // Low priority: sensors, timers
  static unsigned long lastSensorRead = 0;
  if (millis() - lastSensorRead > 1000) {
    lastSensorRead = millis();
    int temp = analogRead(TEMP_SENSOR_PIN);
    return Action::temperatureRead(temp);
  }
  
  // Always return something
  return Action::tick();
}
```

### 5. State Machine Validation

Prevent invalid transitions:

```cpp
bool validTransition(AppMode from, AppMode to) {
  switch (from) {
    case MODE_IDLE:
      return to == MODE_RUNNING || to == MODE_SETUP;
    case MODE_RUNNING:
      return to == MODE_IDLE || to == MODE_ERROR;
    case MODE_ERROR:
      return to == MODE_IDLE;  // can only go back to idle
    default:
      return false;
  }
}

// Use in reducer
case ACTION_CHANGE_MODE:
  if (validTransition(state.mode, action.newMode)) {
    newState.mode = action.newMode;
  }
  break;
```

### 6. Side Effects That Chain

Sometimes one side effect triggers another action:

```cpp
Action handleSideEffects(const AppState& oldState, const AppState& newState) {
  // Update LEDs immediately
  if (newState.mode != oldState.mode) {
    updateStatusLED(newState.mode);
  }
  
  // Start WiFi connection (might need follow-up)
  if (newState.shouldConnect && !oldState.shouldConnect) {
    WiFi.begin(newState.ssid, newState.password);
    return Action::connectionStarted();  // clears the flag
  }
  
  // Save settings in background
  if (newState.settingsChanged && !oldState.settingsChanged) {
    saveToEEPROM(newState.settings);
  }
  
  return Action::none();
}
```

### 7. Button Debouncing

Cleaner button handling:

```cpp
struct Button {
  int pin;
  bool lastState;
  unsigned long lastChange;
  
  Button(int p) : pin(p), lastState(HIGH), lastChange(0) {
    pinMode(pin, INPUT_PULLUP);
  }
  
  bool wasPressed() {
    bool currentState = digitalRead(pin);
    
    if (currentState != lastState) {
      lastChange = millis();
    }
    
    if (millis() - lastChange > 50) {  // debounce delay
      if (currentState != lastState) {
        lastState = currentState;
        return currentState == LOW;  // button pressed
      }
    }
    
    return false;
  }
};
```

### 8. Error Handling

Simple error tracking:

```cpp
enum ErrorCode {
  ERROR_NONE,
  ERROR_WIFI_FAILED,
  ERROR_SENSOR_TIMEOUT,
  ERROR_STORAGE_FULL
};

struct AppState {
  ErrorCode error;
  unsigned long errorTime;
  
  void setError(ErrorCode code) {
    error = code;
    errorTime = millis();
  }
  
  void clearError() {
    error = ERROR_NONE;
  }
  
  bool hasError() {
    return error != ERROR_NONE;
  }
};

// In reducer
case ACTION_WIFI_FAILED:
  newState.setError(ERROR_WIFI_FAILED);
  newState.mode = MODE_DISCONNECTED;
  break;

case ACTION_ERROR_ACKNOWLEDGED:
  newState.clearError();
  break;
```

### 9. Async Operations

Track long-running operations:

```cpp
struct AsyncOp {
  bool active;
  unsigned long startTime;
  unsigned long timeout;
  
  AsyncOp() : active(false), startTime(0), timeout(0) {}
  
  void start(unsigned long timeoutMs) {
    active = true;
    startTime = millis();
    timeout = timeoutMs;
  }
  
  void finish() {
    active = false;
  }
  
  bool timedOut() {
    return active && (millis() - startTime > timeout);
  }
};

// In your state
struct AppState {
  AsyncOp wifiConnection;
};

// In reducer
case ACTION_START_WIFI:
  newState.wifiConnection.start(30000);  // 30 second timeout
  break;

case ACTION_WIFI_CONNECTED:
  newState.wifiConnection.finish();
  break;

case ACTION_TICK:
  if (state.wifiConnection.timedOut()) {
    newState.wifiConnection.finish();
    newState.setError(ERROR_WIFI_TIMEOUT);
  }
  break;
```

## Memory Tips

### Use the Stack

```cpp
// Good - stack allocated, automatic cleanup
void processCommand() {
  char buffer[64];
  // use buffer...
  // automatically cleaned up when function exits
}

// Avoid - heap allocation, manual cleanup
void processCommand() {
  char* buffer = malloc(64);
  // use buffer...
  free(buffer);  // easy to forget!
}
```

### Fixed-Size Strings

```cpp
// Good - predictable memory use
struct Config {
  char ssid[32];
  char password[64];
};

// Avoid - dynamic allocation
struct Config {
  String ssid;      // can fragment memory
  String password;
};

// Use String for temporary work only
void handleCommand() {
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  
  // Convert to fixed storage
  if (cmd.startsWith("SSID:")) {
    cmd.substring(5).toCharArray(config.ssid, sizeof(config.ssid));
  }
}
```

### Check Memory Usage

```cpp
void printMemoryInfo() {
  extern char _end;
  extern "C" char *sbrk(int i);
  char *ramstart = (char *)0x20070000;
  char *ramend = (char *)0x20088000;
  char *heapend = sbrk(0);
  
  Serial.print("Free RAM: ");
  Serial.println((int)ramend - (int)heapend);
}
```

## Performance Tips

### Consistent Timing

```cpp
void loop() {
  static unsigned long lastUpdate = 0;
  
  if (millis() - lastUpdate >= 100) {  // 10Hz update rate
    Action action = readEvents(g_state);
    dispatch(action);
    lastUpdate = millis();
  }
  
  // Don't use delay() - let other stuff run
}
```

### Debug Macros

```cpp
#define DEBUG_ENABLED 1

#if DEBUG_ENABLED
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

// Usage
DEBUG_PRINTLN("Starting WiFi connection...");
```

### Efficient String Building

```cpp
// Good - single allocation
void buildStatus(char* buffer, size_t size, const AppState& state) {
  snprintf(buffer, size, "Mode:%d Temp:%d WiFi:%s", 
           state.mode, state.temperature, state.connected ? "ON" : "OFF");
}

// Avoid - multiple allocations
String buildStatus(const AppState& state) {
  String result = "Mode:";
  result += state.mode;      // allocation
  result += " Temp:";        // allocation
  result += state.temperature; // allocation
  // ... more allocations
  return result;
}
```

## Testing

### Test Your Reducer

```cpp
void testWiFiConnection() {
  AppState state;
  state.mode = MODE_CONNECTING;
  
  Action action = Action::wifiConnected();
  AppState result = reduce(state, action);
  
  if (result.mode == MODE_CONNECTED) {
    Serial.println("✓ WiFi connection test passed");
  } else {
    Serial.println("✗ Test failed");
  }
}

void runTests() {
  Serial.println("Running tests...");
  testWiFiConnection();
  // more tests...
  Serial.println("Tests complete");
}
```

### Debug State

```cpp
void printState(const AppState& state) {
  Serial.println("=== State ===");
  Serial.print("Mode: "); Serial.println(state.mode);
  Serial.print("Connected: "); Serial.println(state.connected);
  Serial.print("Error: "); Serial.println(state.error);
  Serial.println("============");
}

// Use when debugging
#if DEBUG_ENABLED
  if (some_condition) {
    printState(g_state);
  }
#endif
```

## Common Mistakes

### Side Effects in Reducers

```cpp
// DON'T do this
AppState reduce(const AppState& state, const Action& action) {
  AppState newState = state;
  
  if (action.type == BUTTON_PRESSED) {
    digitalWrite(LED_PIN, HIGH);  // Side effect! Wrong!
    Serial.println("Button pressed");  // Side effect! Wrong!
    newState.ledOn = true;
  }
  
  return newState;
}

// DO this instead
void handleSideEffects(const AppState& oldState, const AppState& newState) {
  if (newState.ledOn != oldState.ledOn) {
    digitalWrite(LED_PIN, newState.ledOn ? HIGH : LOW);
    Serial.println(newState.ledOn ? "LED on" : "LED off");
  }
}
```

### Missing Default Cases

```cpp
// Always handle the default case
AppState reduce(const AppState& state, const Action& action) {
  AppState newState = state;
  
  switch (action.type) {
    case ACTION_BUTTON:
      newState.buttonPressed = true;
      break;
    default:
      // Explicitly do nothing
      break;
  }
  
  return newState;
}
```

### Blocking Operations

```cpp
// DON'T block the main loop
void loop() {
  if (needsUserInput) {
    String input = getSerialInput();  // This might wait forever!
  }
  // ... rest of loop can't run
}

// DO use state machines instead
enum InputState {
  INPUT_NONE,
  INPUT_WAITING,
  INPUT_READY
};

// Check for input without blocking
Action readEvents() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    return Action::inputReceived(input);
  }
  return Action::tick();
}
```

This stuff gets easier with practice. Start simple and add complexity bit by bit.