# Moore Machine Examples for Arduino

Practical Moore machine patterns for common Arduino projects. Each example demonstrates proper separation of state space Q, input alphabet Σ, transition function δ, and output function λ.

## Example 1: Smart LED Controller

A multi-mode LED controller demonstrating hierarchical state machines.

### State Space Q

Define finite states for LED patterns and control modes:

```cpp
enum LEDPattern {
  LED_OFF,
  LED_SOLID,
  LED_BLINK_SLOW,
  LED_BLINK_FAST,
  LED_FADE,
  LED_PATTERN_COUNT
};

enum ControlMode {
  CONTROL_MANUAL,      // Button control
  CONTROL_WIFI,        // Network commands
  CONTROL_AUTO,        // Light sensor
  CONTROL_MODE_COUNT
};

struct LEDState {
  LEDPattern pattern;
  ControlMode mode;
  uint8_t brightness;        // 0-255
  unsigned long patternStart;
  bool sensorEnabled;
  
  LEDState() : pattern(LED_OFF), mode(CONTROL_MANUAL), 
               brightness(128), patternStart(0), sensorEnabled(false) {}
};
```

### Input Alphabet Σ

All possible events in the system:

```cpp
enum InputType {
  INPUT_NONE,
  INPUT_BUTTON_MODE,      // Mode button pressed
  INPUT_BUTTON_PATTERN,   // Pattern button pressed  
  INPUT_BUTTON_BRIGHT_UP,
  INPUT_BUTTON_BRIGHT_DOWN,
  INPUT_WIFI_COMMAND,     // Network command received
  INPUT_SENSOR_READING,   // Light sensor data
  INPUT_TIMER_TICK,
  INPUT_COUNT
};

struct Input {
  InputType type;
  
  union {
    struct {
      char command[16];
      int value;
    } wifi;
    int sensorValue;
    int brightness;
  };
  
  Input() : type(INPUT_NONE) {}
  
  static Input buttonMode() {
    Input i; i.type = INPUT_BUTTON_MODE; return i;
  }
  
  static Input wifiCommand(const char* cmd, int val) {
    Input i; 
    i.type = INPUT_WIFI_COMMAND;
    strncpy(i.wifi.command, cmd, 15);
    i.wifi.value = val;
    return i;
  }
  
  static Input sensorReading(int value) {
    Input i; i.type = INPUT_SENSOR_READING; i.sensorValue = value; return i;
  }
};
```

### Transition Function δ: Q × Σ → Q

Pure state transitions with no side effects:

```cpp
LEDState transitionFunction(const LEDState& state, const Input& input) {
  LEDState newState = state;
  
  switch (input.type) {
    case INPUT_BUTTON_MODE:
      // Cycle through control modes
      newState.mode = (ControlMode)((state.mode + 1) % CONTROL_MODE_COUNT);
      break;
      
    case INPUT_BUTTON_PATTERN:
      if (state.mode == CONTROL_MANUAL) {
        // Cycle LED patterns
        newState.pattern = (LEDPattern)((state.pattern + 1) % LED_PATTERN_COUNT);
        newState.patternStart = millis();
      }
      break;
      
    case INPUT_BUTTON_BRIGHT_UP:
      if (state.mode == CONTROL_MANUAL) {
        newState.brightness = min(255, state.brightness + 25);
      }
      break;
      
    case INPUT_BUTTON_BRIGHT_DOWN:
      if (state.mode == CONTROL_MANUAL) {
        newState.brightness = max(0, state.brightness - 25);
      }
      break;
      
    case INPUT_WIFI_COMMAND:
      if (state.mode == CONTROL_WIFI) {
        if (strcmp(input.wifi.command, "pattern") == 0) {
          if (input.wifi.value < LED_PATTERN_COUNT) {
            newState.pattern = (LEDPattern)input.wifi.value;
            newState.patternStart = millis();
          }
        } else if (strcmp(input.wifi.command, "brightness") == 0) {
          newState.brightness = constrain(input.wifi.value, 0, 255);
        }
      }
      break;
      
    case INPUT_SENSOR_READING:
      if (state.mode == CONTROL_AUTO) {
        // Auto-adjust brightness based on ambient light
        int targetBrightness = map(input.sensorValue, 0, 1023, 255, 50);
        newState.brightness = targetBrightness;
        
        // Auto pattern selection
        if (input.sensorValue < 100) {
          newState.pattern = LED_SOLID;  // Dark = solid
        } else if (input.sensorValue < 500) {
          newState.pattern = LED_BLINK_SLOW;  // Medium = slow blink
        } else {
          newState.pattern = LED_OFF;  // Bright = off
        }
      }
      break;
      
    case INPUT_TIMER_TICK:
      // No state changes on tick - just for timing
      break;
  }
  
  return newState;
}
```

### Output Function λ: Q → Γ

Handle all I/O based on current state (Moore property):

```cpp
Input outputFunction(const LEDState& oldState, const LEDState& newState) {
  const int LED_PIN = 9;  // PWM pin
  
  // Calculate LED output based on current state
  int ledValue = 0;
  unsigned long elapsed = millis() - newState.patternStart;
  
  switch (newState.pattern) {
    case LED_OFF:
      ledValue = 0;
      break;
      
    case LED_SOLID:
      ledValue = newState.brightness;
      break;
      
    case LED_BLINK_SLOW:
      ledValue = (elapsed / 1000) % 2 ? newState.brightness : 0;
      break;
      
    case LED_BLINK_FAST:
      ledValue = (elapsed / 200) % 2 ? newState.brightness : 0;
      break;
      
    case LED_FADE:
      // Sine wave fade
      float phase = (elapsed % 3000) / 3000.0 * 2 * PI;
      ledValue = (sin(phase) + 1) * newState.brightness / 2;
      break;
  }
  
  analogWrite(LED_PIN, ledValue);
  
  // Status output when mode changes
  if (oldState.mode != newState.mode) {
    Serial.print("Control mode: ");
    switch (newState.mode) {
      case CONTROL_MANUAL: Serial.println("MANUAL"); break;
      case CONTROL_WIFI: Serial.println("WIFI"); break;
      case CONTROL_AUTO: Serial.println("AUTO"); break;
    }
  }
  
  return Input::none();
}
```

### Complete System Setup

```cpp
#include <MooreArduino.h>
using namespace MooreArduino;

MooreMachine<LEDState, Input> ledMachine(transitionFunction, LEDState());
Timer sensorTimer(500);  // Read sensor every 500ms
Button modeButton(2);
Button patternButton(3);

void setup() {
  Serial.begin(115200);
  
  ledMachine.setOutputFunction(outputFunction);
  sensorTimer.start();
  
  Serial.println("Smart LED Controller - Moore Machine");
}

void loop() {
  Input input = Input::none();
  
  if (modeButton.wasPressed()) {
    input = Input::buttonMode();
  } else if (patternButton.wasPressed()) {
    input = Input::buttonPattern();
  } else if (sensorTimer.expired()) {
    sensorTimer.restart();
    input = Input::sensorReading(analogRead(A0));
  }
  
  if (input.type != INPUT_NONE) {
    ledMachine.step(input);
  }
  
  delay(10);
}
```

## Example 2: Temperature Monitor

Environmental monitoring with alarms and data logging.

### State Space Q

```cpp
enum MonitorMode {
  MONITOR_IDLE,
  MONITOR_SAMPLING,
  MONITOR_ALARM_HIGH,
  MONITOR_ALARM_LOW,
  MONITOR_CALIBRATING,
  MONITOR_MODE_COUNT
};

struct TempState {
  MonitorMode mode;
  float currentTemp;
  float highThreshold;
  float lowThreshold;
  unsigned long lastSample;
  int sampleCount;
  bool alarmEnabled;
  
  TempState() : mode(MONITOR_IDLE), currentTemp(20.0), 
                highThreshold(30.0), lowThreshold(10.0),
                lastSample(0), sampleCount(0), alarmEnabled(true) {}
};
```

### Input Alphabet Σ

```cpp
enum TempInputType {
  TEMP_INPUT_NONE,
  TEMP_INPUT_START_MONITORING,
  TEMP_INPUT_STOP_MONITORING,
  TEMP_INPUT_SENSOR_READING,
  TEMP_INPUT_CALIBRATE,
  TEMP_INPUT_SET_THRESHOLD,
  TEMP_INPUT_ALARM_ACK,
  TEMP_INPUT_TIMER_TICK
};

struct TempInput {
  TempInputType type;
  float temperature;
  float threshold;
  bool isHigh;  // true for high threshold, false for low
  
  static TempInput sensorReading(float temp) {
    TempInput i; i.type = TEMP_INPUT_SENSOR_READING; i.temperature = temp; return i;
  }
  
  static TempInput setThreshold(float thresh, bool high) {
    TempInput i; i.type = TEMP_INPUT_SET_THRESHOLD; 
    i.threshold = thresh; i.isHigh = high; return i;
  }
};
```

### Transition Function δ

```cpp
TempState tempTransition(const TempState& state, const TempInput& input) {
  TempState newState = state;
  
  switch (state.mode) {
    case MONITOR_IDLE:
      if (input.type == TEMP_INPUT_START_MONITORING) {
        newState.mode = MONITOR_SAMPLING;
        newState.sampleCount = 0;
      } else if (input.type == TEMP_INPUT_CALIBRATE) {
        newState.mode = MONITOR_CALIBRATING;
      }
      break;
      
    case MONITOR_SAMPLING:
      if (input.type == TEMP_INPUT_STOP_MONITORING) {
        newState.mode = MONITOR_IDLE;
      } else if (input.type == TEMP_INPUT_SENSOR_READING) {
        newState.currentTemp = input.temperature;
        newState.lastSample = millis();
        newState.sampleCount++;
        
        // Check thresholds
        if (state.alarmEnabled) {
          if (input.temperature > state.highThreshold) {
            newState.mode = MONITOR_ALARM_HIGH;
          } else if (input.temperature < state.lowThreshold) {
            newState.mode = MONITOR_ALARM_LOW;
          }
        }
      }
      break;
      
    case MONITOR_ALARM_HIGH:
    case MONITOR_ALARM_LOW:
      if (input.type == TEMP_INPUT_ALARM_ACK) {
        newState.mode = MONITOR_SAMPLING;  // Return to sampling
      } else if (input.type == TEMP_INPUT_SENSOR_READING) {
        newState.currentTemp = input.temperature;
        // Stay in alarm state until acknowledged
      }
      break;
      
    case MONITOR_CALIBRATING:
      if (input.type == TEMP_INPUT_SENSOR_READING) {
        // Simple calibration - could be more sophisticated
        newState.currentTemp = input.temperature;
        newState.mode = MONITOR_IDLE;  // Calibration complete
      }
      break;
  }
  
  // Global transitions
  if (input.type == TEMP_INPUT_SET_THRESHOLD) {
    if (input.isHigh) {
      newState.highThreshold = input.threshold;
    } else {
      newState.lowThreshold = input.threshold;
    }
  }
  
  return newState;
}
```

### Output Function λ

```cpp
TempInput tempOutputFunction(const TempState& oldState, const TempState& newState) {
  const int ALARM_LED = 4;
  const int STATUS_LED = 5;
  
  // Moore property: outputs depend only on current state
  switch (newState.mode) {
    case MONITOR_IDLE:
      digitalWrite(STATUS_LED, LOW);
      digitalWrite(ALARM_LED, LOW);
      break;
      
    case MONITOR_SAMPLING:
      digitalWrite(STATUS_LED, HIGH);
      digitalWrite(ALARM_LED, LOW);
      break;
      
    case MONITOR_ALARM_HIGH:
    case MONITOR_ALARM_LOW:
      digitalWrite(STATUS_LED, HIGH);
      digitalWrite(ALARM_LED, (millis() / 250) % 2);  // Blink alarm
      break;
      
    case MONITOR_CALIBRATING:
      digitalWrite(STATUS_LED, (millis() / 100) % 2);  // Fast blink
      digitalWrite(ALARM_LED, LOW);
      break;
  }
  
  // State entry actions
  if (oldState.mode != newState.mode) {
    Serial.print("Monitor mode: ");
    Serial.println(newState.mode);
    
    if (newState.mode == MONITOR_ALARM_HIGH) {
      Serial.print("HIGH TEMP ALARM: ");
      Serial.println(newState.currentTemp);
    } else if (newState.mode == MONITOR_ALARM_LOW) {
      Serial.print("LOW TEMP ALARM: ");
      Serial.println(newState.currentTemp);
    }
  }
  
  // Log temperature readings
  if (newState.mode == MONITOR_SAMPLING && 
      newState.sampleCount != oldState.sampleCount) {
    Serial.print("Temp: ");
    Serial.print(newState.currentTemp);
    Serial.print("°C (Sample #");
    Serial.print(newState.sampleCount);
    Serial.println(")");
  }
  
  return TempInput::none();
}
```

## Example 3: Garden Watering System

Automated irrigation with moisture sensors and scheduling.

### State Space Q

```cpp
enum WaterMode {
  WATER_IDLE,
  WATER_SENSING,
  WATER_WATERING,
  WATER_SCHEDULED,
  WATER_MANUAL,
  WATER_ERROR,
  WATER_MODE_COUNT
};

struct WaterState {
  WaterMode mode;
  int moistureLevel;      // 0-1023
  int targetMoisture;     // Desired moisture level
  unsigned long waterStartTime;
  unsigned long waterDuration;
  unsigned long nextSchedule;
  bool valveOpen;
  int errorCode;
  
  WaterState() : mode(WATER_IDLE), moistureLevel(500), targetMoisture(600),
                 waterStartTime(0), waterDuration(30000), // 30 seconds
                 nextSchedule(0), valveOpen(false), errorCode(0) {}
};
```

### Advanced State Machine Features

```cpp
WaterState waterTransition(const WaterState& state, const WaterInput& input) {
  WaterState newState = state;
  
  switch (state.mode) {
    case WATER_IDLE:
      if (input.type == WATER_INPUT_START_AUTO) {
        newState.mode = WATER_SENSING;
      } else if (input.type == WATER_INPUT_MANUAL_ON) {
        newState.mode = WATER_MANUAL;
        newState.waterStartTime = millis();
      } else if (input.type == WATER_INPUT_SCHEDULE) {
        newState.mode = WATER_SCHEDULED;
        newState.nextSchedule = millis() + input.scheduleDelay;
      }
      break;
      
    case WATER_SENSING:
      if (input.type == WATER_INPUT_MOISTURE_READING) {
        newState.moistureLevel = input.moistureValue;
        
        if (input.moistureValue < state.targetMoisture) {
          newState.mode = WATER_WATERING;
          newState.waterStartTime = millis();
          newState.valveOpen = true;
        }
      } else if (input.type == WATER_INPUT_STOP) {
        newState.mode = WATER_IDLE;
      }
      break;
      
    case WATER_WATERING:
      if (input.type == WATER_INPUT_TIMER_TICK) {
        unsigned long elapsed = millis() - state.waterStartTime;
        if (elapsed > state.waterDuration) {
          newState.mode = WATER_SENSING;  // Return to sensing
          newState.valveOpen = false;
        }
      } else if (input.type == WATER_INPUT_STOP) {
        newState.mode = WATER_IDLE;
        newState.valveOpen = false;
      }
      break;
      
    case WATER_SCHEDULED:
      if (input.type == WATER_INPUT_TIMER_TICK) {
        if (millis() >= state.nextSchedule) {
          newState.mode = WATER_SENSING;
        }
      }
      break;
      
    case WATER_MANUAL:
      if (input.type == WATER_INPUT_MANUAL_OFF || 
          input.type == WATER_INPUT_STOP) {
        newState.mode = WATER_IDLE;
        newState.valveOpen = false;
      } else if (input.type == WATER_INPUT_TIMER_TICK) {
        // Safety timeout for manual mode
        unsigned long elapsed = millis() - state.waterStartTime;
        if (elapsed > 300000) {  // 5 minute safety limit
          newState.mode = WATER_ERROR;
          newState.errorCode = 1;  // Manual timeout
          newState.valveOpen = false;
        }
      }
      break;
      
    case WATER_ERROR:
      if (input.type == WATER_INPUT_RESET) {
        newState.mode = WATER_IDLE;
        newState.errorCode = 0;
      }
      break;
  }
  
  return newState;
}
```

## Common Patterns Summary

### 1. Hierarchical States
Break complex systems into subsystems with their own state machines.

### 2. Timeout Handling
Use timer inputs and state entry times for automatic transitions.

### 3. Error Recovery
Design explicit error states with recovery mechanisms.

### 4. Safety Limits
Implement bounds checking and safety timeouts.

### 5. Mode Switching
Allow systems to operate in different modes with different behaviors.

### 6. Scheduled Operations
Use absolute timestamps for scheduling future events.

### 7. Sensor Integration
Process sensor readings as inputs to drive state transitions.

### 8. User Interface
Map physical buttons and commands to input symbols.

These examples demonstrate how Moore machines provide structure and predictability to embedded systems while maintaining the mathematical properties that make them reliable and testable.