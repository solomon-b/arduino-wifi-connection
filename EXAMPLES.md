# Real-World Examples

Practical Redux patterns for common Arduino projects.

## Example 1: Smart LED Controller

Multiple modes, brightness control, and WiFi commands.

### The State

What does this thing need to remember?

```cpp
enum LedPattern {
  PATTERN_OFF,
  PATTERN_SOLID,
  PATTERN_BLINK,
  PATTERN_FADE
};

enum ControlMode {
  MODE_BUTTONS,    // Physical buttons
  MODE_WIFI,       // WiFi commands
  MODE_AUTO        // Light sensor
};

struct LedState {
  LedPattern pattern;
  ControlMode mode;
  int brightness;      // 0-255
  int speed;          // 1-10
  bool isOn;
  int lightSensor;    // Raw sensor value
  unsigned long lastPatternUpdate;
  
  // Reasonable defaults
  LedState() : pattern(PATTERN_OFF), mode(MODE_BUTTONS), brightness(128), 
               speed(5), isOn(false), lightSensor(0), lastPatternUpdate(0) {}
};
```

### The Actions

```cpp
enum LedActionType {
  ACTION_POWER_TOGGLE,
  ACTION_NEXT_PATTERN,
  ACTION_BRIGHTER,
  ACTION_DIMMER,
  ACTION_SPEED_UP,
  ACTION_SPEED_DOWN,
  ACTION_CHANGE_MODE,
  ACTION_SENSOR_READ,
  ACTION_WIFI_COMMAND,
  ACTION_TICK
};

struct LedAction {
  LedActionType type;
  int value;
  ControlMode newMode;
  String command;
  
  static LedAction powerToggle() {
    LedAction a;
    a.type = ACTION_POWER_TOGGLE;
    return a;
  }
  
  static LedAction brighter() {
    LedAction a;
    a.type = ACTION_BRIGHTER;
    return a;
  }
  
  static LedAction sensorRead(int value) {
    LedAction a;
    a.type = ACTION_SENSOR_READ;
    a.value = value;
    return a;
  }
  
  static LedAction wifiCommand(const String& cmd) {
    LedAction a;
    a.type = ACTION_WIFI_COMMAND;
    a.command = cmd;
    return a;
  }
  
  // ... more factory methods
};
```

### The Reducer

```cpp
LedState reduce(const LedState& state, const LedAction& action) {
  LedState newState = state;
  
  switch (action.type) {
    case ACTION_POWER_TOGGLE:
      newState.isOn = !state.isOn;
      if (!newState.isOn) {
        newState.pattern = PATTERN_OFF;
      }
      break;
      
    case ACTION_NEXT_PATTERN:
      if (state.isOn) {
        int next = (state.pattern + 1) % 4;
        newState.pattern = (LedPattern)next;
      }
      break;
      
    case ACTION_BRIGHTER:
      if (state.isOn) {
        newState.brightness = min(255, state.brightness + 25);
      }
      break;
      
    case ACTION_DIMMER:
      if (state.isOn) {
        newState.brightness = max(10, state.brightness - 25);
      }
      break;
      
    case ACTION_SENSOR_READ:
      newState.lightSensor = action.value;
      // Auto mode: dimmer when it's bright outside
      if (state.mode == MODE_AUTO && state.isOn) {
        newState.brightness = map(action.value, 0, 1023, 255, 50);
      }
      break;
      
    case ACTION_WIFI_COMMAND:
      // Handle commands like "ON", "OFF", "BRIGHTNESS 200", "PATTERN FADE"
      if (action.command == "ON") {
        newState.isOn = true;
        newState.pattern = PATTERN_SOLID;
      } else if (action.command == "OFF") {
        newState.isOn = false;
        newState.pattern = PATTERN_OFF;
      } else if (action.command.startsWith("BRIGHTNESS ")) {
        int value = action.command.substring(11).toInt();
        newState.brightness = constrain(value, 0, 255);
      }
      break;
      
    case ACTION_TICK:
      // Update pattern timing
      if (state.isOn && state.pattern != PATTERN_OFF && 
          millis() - state.lastPatternUpdate > (1100 - state.speed * 100)) {
        newState.lastPatternUpdate = millis();
      }
      break;
  }
  
  return newState;
}
```

### Hardware Updates

```cpp
void updateLED(const LedState& oldState, const LedState& newState) {
  // Only update when something actually changed
  if (newState.pattern != oldState.pattern || 
      newState.brightness != oldState.brightness ||
      newState.lastPatternUpdate != oldState.lastPatternUpdate) {
    
    int ledValue = 0;
    
    switch (newState.pattern) {
      case PATTERN_OFF:
        ledValue = 0;
        break;
      case PATTERN_SOLID:
        ledValue = newState.brightness;
        break;
      case PATTERN_BLINK:
        ledValue = (millis() / 500) % 2 ? newState.brightness : 0;
        break;
      case PATTERN_FADE:
        float fadePhase = (millis() % 2000) / 2000.0;
        ledValue = (sin(fadePhase * PI) * newState.brightness);
        break;
    }
    
    analogWrite(LED_PIN, ledValue);
  }
}
```

### Reading Events

```cpp
LedAction readLedEvents() {
  // Button inputs
  static Button powerBtn(2);
  static Button patternBtn(3);
  static Button brighterBtn(4);
  static Button dimmerBtn(5);
  
  if (powerBtn.wasPressed()) return LedAction::powerToggle();
  if (patternBtn.wasPressed()) return LedAction::nextPattern();
  if (brighterBtn.wasPressed()) return LedAction::brighter();
  if (dimmerBtn.wasPressed()) return LedAction::dimmer();
  
  // Light sensor (every 500ms)
  static unsigned long lastSensorRead = 0;
  if (millis() - lastSensorRead > 500) {
    lastSensorRead = millis();
    int sensorValue = analogRead(A0);
    return LedAction::sensorRead(sensorValue);
  }
  
  // WiFi commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    return LedAction::wifiCommand(cmd);
  }
  
  return LedAction::tick();
}
```

## Example 2: Temperature Monitor

Track temperature, log data, send alerts.

### State Design

```cpp
struct TempReading {
  float temperature;
  unsigned long timestamp;
  
  TempReading() : temperature(0), timestamp(0) {}
  
  bool isValid() {
    return temperature > -40 && temperature < 80;
  }
};

enum AlertLevel {
  ALERT_NONE,
  ALERT_WARM,
  ALERT_HOT,
  ALERT_COLD
};

struct MonitorState {
  TempReading current;
  TempReading history[24];  // Last 24 readings
  int historyIndex;
  float alertThresholds[3]; // cold, warm, hot
  AlertLevel currentAlert;
  bool alertMuted;
  Timer readingTimer;
  Timer logTimer;
  
  MonitorState() : historyIndex(0), currentAlert(ALERT_NONE), alertMuted(false),
                   readingTimer(5000), logTimer(3600000) {
    // Default thresholds: 10°C cold, 25°C warm, 35°C hot
    alertThresholds[0] = 10.0;
    alertThresholds[1] = 25.0;
    alertThresholds[2] = 35.0;
    readingTimer.start();
    logTimer.start();
  }
};
```

### Simple Actions

```cpp
enum MonitorActionType {
  ACTION_TEMP_READ,
  ACTION_LOG_DATA,
  ACTION_MUTE_ALERT,
  ACTION_UNMUTE_ALERT,
  ACTION_CHECK_ALERTS,
  ACTION_TICK
};

struct MonitorAction {
  MonitorActionType type;
  float temperature;
  
  static MonitorAction tempRead(float temp) {
    MonitorAction a;
    a.type = ACTION_TEMP_READ;
    a.temperature = temp;
    return a;
  }
  
  static MonitorAction muteAlert() {
    MonitorAction a;
    a.type = ACTION_MUTE_ALERT;
    return a;
  }
  
  static MonitorAction tick() {
    MonitorAction a;
    a.type = ACTION_TICK;
    return a;
  }
};
```

### Alert Logic

```cpp
AlertLevel checkAlertLevel(float temp, const float thresholds[]) {
  if (temp < thresholds[0]) return ALERT_COLD;
  if (temp > thresholds[2]) return ALERT_HOT;
  if (temp > thresholds[1]) return ALERT_WARM;
  return ALERT_NONE;
}

MonitorState reduce(const MonitorState& state, const MonitorAction& action) {
  MonitorState newState = state;
  
  switch (action.type) {
    case ACTION_TEMP_READ:
      if (action.temperature > -40 && action.temperature < 80) {
        newState.current.temperature = action.temperature;
        newState.current.timestamp = millis();
      }
      break;
      
    case ACTION_LOG_DATA:
      if (state.current.isValid()) {
        newState.history[state.historyIndex] = state.current;
        newState.historyIndex = (state.historyIndex + 1) % 24;
        newState.logTimer.restart();
      }
      break;
      
    case ACTION_CHECK_ALERTS:
      if (state.current.isValid()) {
        AlertLevel level = checkAlertLevel(state.current.temperature, state.alertThresholds);
        newState.currentAlert = level;
      }
      break;
      
    case ACTION_MUTE_ALERT:
      newState.alertMuted = true;
      break;
      
    case ACTION_UNMUTE_ALERT:
      newState.alertMuted = false;
      break;
      
    case ACTION_TICK:
      // Timer management happens in side effects
      break;
  }
  
  return newState;
}
```

### Side Effects

```cpp
Action handleMonitorSideEffects(const MonitorState& oldState, const MonitorState& newState) {
  // Sound alert when level changes (and not muted)
  if (newState.currentAlert != oldState.currentAlert && !newState.alertMuted) {
    if (newState.currentAlert != ALERT_NONE) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100);
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
  
  // Update display when temperature changes
  if (newState.current.temperature != oldState.current.temperature) {
    Serial.print("Temperature: ");
    Serial.print(newState.current.temperature);
    Serial.print("°C ");
    
    switch (newState.currentAlert) {
      case ALERT_COLD: Serial.println("(COLD)"); break;
      case ALERT_WARM: Serial.println("(WARM)"); break;
      case ALERT_HOT: Serial.println("(HOT!)"); break;
      default: Serial.println("(OK)"); break;
    }
  }
  
  // Check timers
  if (newState.readingTimer.expired()) {
    return MonitorAction::tempRead(readTemperatureSensor());
  }
  
  if (newState.logTimer.expired()) {
    return MonitorAction::logData();
  }
  
  return MonitorAction::tick();
}
```

## Example 3: Garden Watering System

Multiple zones, soil sensors, weather data, manual override.

### Complex State

```cpp
struct Zone {
  int soilMoisture;      // 0-100%
  int threshold;         // Water when below this
  bool isWatering;
  unsigned long waterStart;
  unsigned long maxWaterTime;  // Safety limit
  
  Zone() : soilMoisture(50), threshold(30), isWatering(false), 
           waterStart(0), maxWaterTime(300000) {}  // 5 min max
};

enum WeatherType {
  WEATHER_UNKNOWN,
  WEATHER_SUNNY,
  WEATHER_CLOUDY,
  WEATHER_RAINY
};

enum WateringMode {
  MODE_OFF,
  MODE_AUTO,
  MODE_MANUAL,
  MODE_SCHEDULE
};

struct GardenState {
  Zone zones[4];
  WateringMode mode;
  WeatherType weather;
  int lightLevel;        // 0-100%
  bool manualOverride;
  unsigned long dailyWaterUsed;  // ml
  Timer sensorTimer;
  Timer scheduleTimer;
  
  GardenState() : mode(MODE_AUTO), weather(WEATHER_UNKNOWN), lightLevel(50),
                  manualOverride(false), dailyWaterUsed(0),
                  sensorTimer(30000), scheduleTimer(3600000) {
    sensorTimer.start();
    scheduleTimer.start();
  }
};
```

### Garden Actions

```cpp
enum GardenActionType {
  ACTION_SENSOR_UPDATE,
  ACTION_WEATHER_UPDATE,
  ACTION_START_WATERING,
  ACTION_STOP_WATERING,
  ACTION_MANUAL_WATER,
  ACTION_SAFETY_CHECK,
  ACTION_MODE_CHANGE,
  ACTION_SCHEDULE_CHECK
};

struct GardenAction {
  GardenActionType type;
  int zoneId;
  int value;
  WateringMode newMode;
  WeatherType newWeather;
  
  static GardenAction sensorUpdate(int zone, int moisture) {
    GardenAction a;
    a.type = ACTION_SENSOR_UPDATE;
    a.zoneId = zone;
    a.value = moisture;
    return a;
  }
  
  static GardenAction startWatering(int zone) {
    GardenAction a;
    a.type = ACTION_START_WATERING;
    a.zoneId = zone;
    return a;
  }
  
  static GardenAction manualWater(int zone) {
    GardenAction a;
    a.type = ACTION_MANUAL_WATER;
    a.zoneId = zone;
    return a;
  }
};
```

### Business Logic

```cpp
bool shouldWater(const Zone& zone, WeatherType weather, int lightLevel) {
  // Don't water if it's raining
  if (weather == WEATHER_RAINY) return false;
  
  // Don't water at night (too dark)
  if (lightLevel < 20) return false;
  
  // Don't water if already watering
  if (zone.isWatering) return false;
  
  // Water if soil is dry
  return zone.soilMoisture < zone.threshold;
}

GardenState reduce(const GardenState& state, const GardenAction& action) {
  GardenState newState = state;
  
  switch (action.type) {
    case ACTION_SENSOR_UPDATE:
      if (action.zoneId >= 0 && action.zoneId < 4) {
        newState.zones[action.zoneId].soilMoisture = action.value;
      }
      break;
      
    case ACTION_WEATHER_UPDATE:
      newState.weather = action.newWeather;
      break;
      
    case ACTION_START_WATERING:
      if (action.zoneId >= 0 && action.zoneId < 4) {
        Zone& zone = newState.zones[action.zoneId];
        if (shouldWater(zone, state.weather, state.lightLevel)) {
          zone.isWatering = true;
          zone.waterStart = millis();
        }
      }
      break;
      
    case ACTION_STOP_WATERING:
      if (action.zoneId >= 0 && action.zoneId < 4) {
        Zone& zone = newState.zones[action.zoneId];
        if (zone.isWatering) {
          zone.isWatering = false;
          // Track water usage (rough estimate)
          unsigned long duration = millis() - zone.waterStart;
          newState.dailyWaterUsed += duration / 1000;  // 1ml per second
        }
      }
      break;
      
    case ACTION_MANUAL_WATER:
      newState.manualOverride = true;
      if (action.zoneId >= 0 && action.zoneId < 4) {
        Zone& zone = newState.zones[action.zoneId];
        if (zone.isWatering) {
          zone.isWatering = false;
        } else {
          zone.isWatering = true;
          zone.waterStart = millis();
        }
      }
      break;
      
    case ACTION_SAFETY_CHECK:
      // Stop any watering that's gone too long
      for (int i = 0; i < 4; i++) {
        Zone& zone = newState.zones[i];
        if (zone.isWatering && millis() - zone.waterStart > zone.maxWaterTime) {
          zone.isWatering = false;
        }
      }
      break;
      
    case ACTION_MODE_CHANGE:
      newState.mode = action.newMode;
      newState.manualOverride = false;
      break;
  }
  
  return newState;
}
```

### Hardware Control

```cpp
void updateWateringHardware(const GardenState& oldState, const GardenState& newState) {
  for (int i = 0; i < 4; i++) {
    if (newState.zones[i].isWatering != oldState.zones[i].isWatering) {
      digitalWrite(VALVE_PINS[i], newState.zones[i].isWatering ? HIGH : LOW);
      
      if (newState.zones[i].isWatering) {
        Serial.print("Started watering zone ");
        Serial.println(i);
      } else {
        Serial.print("Stopped watering zone ");
        Serial.println(i);
      }
    }
  }
}
```

## Key Lessons

### Start Simple
Don't try to build everything at once. Start with basic on/off, then add features.

### Model the Real World
Your state should reflect what's actually happening in the physical world.

### Safety First
Always have timeouts and limits. Hardware can fail.

### Test Small Parts
Test your reducer functions separately before hooking up hardware.

### Use Good Names
`ACTION_EMERGENCY_STOP` is better than `ACTION_BUTTON_1`.

### Handle Errors
What happens when sensors fail? Network drops? Plan for it.

### Keep It Debuggable
Print state changes. Log important events. You'll thank yourself later.

These examples show real patterns you'll use. Start with the LED controller - it's simple but covers the basics. Then move on to more complex stuff as you get comfortable.