# Redux-Style State Management for Arduino

## What This Is

This tutorial shows you how to write Arduino code that doesn't turn into a mess as it gets bigger. Instead of global variables scattered everywhere and spaghetti code, you keep all your state in one place and change it in predictable ways.

It's based on Redux (from JavaScript) and the Elm architecture. If you've used React with Redux, this will look familiar.

## What This Is NOT

This is **not** Functional Reactive Programming (FRP). Real FRP has continuous streams and automatic dependency tracking. This is simpler - just good state management.

## Why Bother?

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

With Redux patterns, you get:
- One place where all state lives
- Predictable ways to change state
- Easy debugging
- Code that stays readable

## The Big Idea

Everything flows in one direction:

```
User presses button → Action → Update state → Update LEDs/screen
Timer expires     → Action → Update state → Check sensors
WiFi connects     → Action → Update state → Show status
```

## Core Concepts

### 1. Put All State in One Place

Instead of scattered variables:

```cpp
// Bad - state everywhere
int temperature;
bool alarm_on;
unsigned long last_reading;
```

Put it all together:

```cpp
// Good - single source of truth
struct AppState {
  int temperature;
  bool alarmOn;
  unsigned long lastReading;
};
AppState g_state;
```

### 2. Use Actions to Describe What Happened

Instead of directly changing variables, describe what happened:

```cpp
enum ActionType {
  BUTTON_PRESSED,
  TEMPERATURE_READ,
  ALARM_TRIGGERED
};

struct Action {
  ActionType type;
  int value;  // for temperature readings, etc.
  
  static Action buttonPressed() {
    Action a;
    a.type = BUTTON_PRESSED;
    return a;
  }
};
```

### 3. Use Pure Functions to Update State

A pure function always returns the same output for the same input:

```cpp
AppState reduce(const AppState& state, const Action& action) {
  AppState newState = state;  // copy current state
  
  switch (action.type) {
    case BUTTON_PRESSED:
      newState.alarmOn = !state.alarmOn;  // toggle alarm
      break;
    case TEMPERATURE_READ:
      newState.temperature = action.value;
      break;
  }
  
  return newState;  // return new state, don't modify old one
}
```

### 4. Handle I/O Separately

Don't mix hardware operations with state logic:

```cpp
void handleSideEffects(const AppState& oldState, const AppState& newState) {
  // Only touch hardware when state actually changed
  if (newState.alarmOn != oldState.alarmOn) {
    digitalWrite(BUZZER_PIN, newState.alarmOn ? HIGH : LOW);
  }
  
  if (newState.temperature != oldState.temperature) {
    Serial.print("Temperature: ");
    Serial.println(newState.temperature);
  }
}
```

## Building Your First Redux Arduino App

Let's build a simple LED controller:

### Step 1: Define Your State

What does your app need to remember?

```cpp
enum LedMode {
  LED_OFF,
  LED_SOLID,
  LED_BLINK
};

struct LedState {
  LedMode mode;
  int brightness;
  unsigned long lastBlink;
  
  LedState() : mode(LED_OFF), brightness(128), lastBlink(0) {}
};
```

### Step 2: Define Your Actions

What can happen to change the state?

```cpp
enum LedActionType {
  BUTTON_PRESS,
  TIMER_TICK,
  BRIGHTNESS_CHANGE
};

struct LedAction {
  LedActionType type;
  int value;
  
  static LedAction buttonPress() {
    LedAction a;
    a.type = BUTTON_PRESS;
    return a;
  }
  
  static LedAction tick() {
    LedAction a;
    a.type = TIMER_TICK;
    return a;
  }
  
  static LedAction setBrightness(int brightness) {
    LedAction a;
    a.type = BRIGHTNESS_CHANGE;
    a.value = brightness;
    return a;
  }
};
```

### Step 3: Write Your Reducer

How does each action change the state?

```cpp
LedState reduce(const LedState& state, const LedAction& action) {
  LedState newState = state;
  
  switch (action.type) {
    case BUTTON_PRESS:
      // Cycle through modes
      if (state.mode == LED_OFF) newState.mode = LED_SOLID;
      else if (state.mode == LED_SOLID) newState.mode = LED_BLINK;
      else newState.mode = LED_OFF;
      break;
      
    case TIMER_TICK:
      if (state.mode == LED_BLINK && millis() - state.lastBlink > 500) {
        newState.lastBlink = millis();
        // The LED toggle will happen in side effects
      }
      break;
      
    case BRIGHTNESS_CHANGE:
      newState.brightness = constrain(action.value, 0, 255);
      break;
  }
  
  return newState;
}
```

### Step 4: Handle Hardware

Update LEDs and other hardware based on state changes:

```cpp
void handleLedSideEffects(const LedState& oldState, const LedState& newState) {
  bool ledShouldBeOn = false;
  
  switch (newState.mode) {
    case LED_SOLID:
      ledShouldBeOn = true;
      break;
    case LED_BLINK:
      ledShouldBeOn = (millis() / 500) % 2;
      break;
    case LED_OFF:
      ledShouldBeOn = false;
      break;
  }
  
  if (ledShouldBeOn) {
    analogWrite(LED_PIN, newState.brightness);
  } else {
    analogWrite(LED_PIN, 0);
  }
}
```

### Step 5: Read Events

Convert button presses and timers into actions:

```cpp
LedAction readEvents() {
  static bool lastButtonState = HIGH;
  bool buttonState = digitalRead(BUTTON_PIN);
  
  // Button pressed (with simple debouncing)
  if (lastButtonState == HIGH && buttonState == LOW) {
    lastButtonState = buttonState;
    delay(50);  // simple debounce
    return LedAction::buttonPress();
  }
  lastButtonState = buttonState;
  
  // Always tick to handle timing
  return LedAction::tick();
}
```

### Step 6: Put It All Together

```cpp
LedState g_ledState;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.begin(9600);
}

void loop() {
  LedAction action = readEvents();
  
  LedState oldState = g_ledState;
  g_ledState = reduce(g_ledState, action);
  
  handleLedSideEffects(oldState, g_ledState);
  
  delay(10);
}
```

## Why This Works

**Predictable**: Same action + same state = same result, every time.

**Debuggable**: Print the action and state to see exactly what's happening.

**Testable**: Your reducer is just a function - easy to test.

**Scalable**: Add new features by adding actions and updating the reducer.

## Common Patterns

### Timers

```cpp
struct Timer {
  unsigned long interval;
  unsigned long lastTrigger;
  bool active;
  
  Timer(unsigned long ms) : interval(ms), lastTrigger(0), active(false) {}
  
  void start() {
    lastTrigger = millis();
    active = true;
  }
  
  bool expired() {
    return active && (millis() - lastTrigger >= interval);
  }
  
  void reset() { active = false; }
};
```

### Button Debouncing

```cpp
struct Button {
  int pin;
  bool lastState;
  unsigned long lastChange;
  
  Button(int p) : pin(p), lastState(HIGH), lastChange(0) {
    pinMode(pin, INPUT_PULLUP);
  }
  
  bool pressed() {
    bool currentState = digitalRead(pin);
    if (currentState != lastState && millis() - lastChange > 50) {
      lastState = currentState;
      lastChange = millis();
      return currentState == LOW;  // pressed
    }
    return false;
  }
};
```

### State Machine Validation

```cpp
bool validTransition(AppMode from, AppMode to) {
  switch (from) {
    case MODE_IDLE:
      return to == MODE_RUNNING || to == MODE_SETUP;
    case MODE_RUNNING:
      return to == MODE_IDLE || to == MODE_ERROR;
    case MODE_ERROR:
      return to == MODE_IDLE;
    default:
      return false;
  }
}
```

## Tips

**Keep state minimal**: Only store what you can't calculate from other data.

**Make actions descriptive**: `RETRY_CONNECTION` is better than `SET_FLAG_TRUE`.

**Don't put I/O in reducers**: Keep them pure for easy testing.

**Use enums**: They're safer than magic numbers.

**Start simple**: Add complexity gradually.

## Memory Management

Arduino has limited RAM. Some tips:

**Use stack allocation**:
```cpp
void processData() {
  char buffer[64];  // allocated on stack, automatically cleaned up
  // don't use malloc/new unless you have to
}
```

**Fixed-size strings**:
```cpp
struct Config {
  char ssid[32];     // fixed size, predictable memory use
  char password[64];
};
```

**Avoid String objects in state**:
```cpp
// Use for temporary operations only
String command = Serial.readStringUntil('\n');
command.trim();
// Then convert to char array for storage
command.toCharArray(config.ssid, sizeof(config.ssid));
```

## Common Mistakes

**Side effects in reducers**:
```cpp
// DON'T do this
AppState reduce(const AppState& state, const Action& action) {
  AppState newState = state;
  if (action.type == BUTTON_PRESSED) {
    digitalWrite(LED_PIN, HIGH);  // Side effect! Wrong place!
    newState.ledOn = true;
  }
  return newState;
}
```

**Forgetting the default case**:
```cpp
// Always handle unknown actions
switch (action.type) {
  case ACTION_ONE:
    // handle it
    break;
  default:
    // do nothing, but be explicit about it
    break;
}
```

**Blocking in the main loop**:
```cpp
// Don't do this
void loop() {
  if (needsCredentials) {
    // This blocks everything else!
    String ssid = promptForSSID();  // blocking call
  }
  // ... rest of loop
}
```

## Testing Your Reducer

```cpp
void testButtonToggle() {
  AppState state;
  state.ledOn = false;
  
  Action action = Action::buttonPressed();
  AppState result = reduce(state, action);
  
  if (result.ledOn) {
    Serial.println("✓ Button toggle test passed");
  } else {
    Serial.println("✗ Button toggle test failed");
  }
}
```

This pattern makes Arduino code much more manageable. Start with something simple and grow from there. Your future self will thank you.