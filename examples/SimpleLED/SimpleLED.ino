/*
 * Simple LED Controller using MooreArduino Library
 * 
 * This example demonstrates pure Moore machine patterns with the MooreArduino library.
 * Controls an LED with a button using predictable state management.
 * 
 * Hardware:
 * - LED on pin 13 (built-in LED on most Arduino boards)
 * - Button on pin 2 (with internal pull-up resistor)
 * 
 * Behavior:
 * - Press button to cycle through LED modes: OFF -> ON -> SLOW_BLINK -> FAST_BLINK -> OFF
 * - LED blinks different patterns based on mode
 * - Serial output shows state changes
 * 
 * Moore Machine Definition:
 * - Q (states): {LED_OFF, LED_ON, LED_BLINKING_SLOW, LED_BLINKING_FAST}
 * - Σ (inputs): {INPUT_NONE, INPUT_BUTTON_PRESSED, INPUT_TICK}
 * - Γ (outputs): {EFFECT_NONE, EFFECT_LED_OFF, EFFECT_LED_ON, EFFECT_LED_BLINK_SLOW, EFFECT_LED_BLINK_FAST, EFFECT_LOG_STATE_CHANGE}
 * - δ (transition): Button press cycles modes, tick advances blink timing
 * - λ (output): Pure function generating effects based on current state
 */

#include <MooreArduino.h>

using namespace MooreArduino;

//----------------------------------------------------------------------------//
// State Space Q, Input Alphabet Σ, and Output Alphabet Γ
//----------------------------------------------------------------------------//

enum LEDMode {
  LED_OFF,
  LED_ON,
  LED_BLINKING_SLOW,
  LED_BLINKING_FAST
};

enum InputType {
  INPUT_NONE,
  INPUT_BUTTON_PRESSED,
  INPUT_TICK
};

enum OutputType {
  EFFECT_NONE,
  EFFECT_LED_OFF,
  EFFECT_LED_ON,
  EFFECT_LED_BLINK_SLOW,
  EFFECT_LED_BLINK_FAST,
  EFFECT_LOG_STATE_CHANGE
};

struct AppState {
  LEDMode mode;
  unsigned long lastUpdate;
  int blinkCounter;
  
  AppState() : mode(LED_OFF), lastUpdate(0), blinkCounter(0) {}
};

struct Input {
  InputType type;
  
  Input() : type(INPUT_NONE) {}
  
  static Input buttonPressed() {
    Input i;
    i.type = INPUT_BUTTON_PRESSED;
    return i;
  }
  
  static Input tick() {
    Input i;
    i.type = INPUT_TICK;
    return i;
  }
  
  static Input none() {
    Input i;
    i.type = INPUT_NONE;
    return i;
  }
};

struct Output {
  OutputType type;
  LEDMode newMode;  // For state change logging
  int blinkState;   // For blink effects
  
  Output() : type(EFFECT_NONE), newMode(LED_OFF), blinkState(0) {}
  
  static Output none() {
    Output e;
    e.type = EFFECT_NONE;
    return e;
  }
  
  static Output ledOff() {
    Output e;
    e.type = EFFECT_LED_OFF;
    return e;
  }
  
  static Output ledOn() {
    Output e;
    e.type = EFFECT_LED_ON;
    return e;
  }
  
  static Output ledBlinkSlow(int counter) {
    Output e;
    e.type = EFFECT_LED_BLINK_SLOW;
    e.blinkState = (counter / 5) % 2;  // Blink every 50 ticks (500ms at 10Hz)
    return e;
  }
  
  static Output ledBlinkFast(int counter) {
    Output e;
    e.type = EFFECT_LED_BLINK_FAST;
    e.blinkState = counter % 2;  // Blink every 10 ticks (100ms at 10Hz)
    return e;
  }
  
  static Output logStateChange(LEDMode mode) {
    Output e;
    e.type = EFFECT_LOG_STATE_CHANGE;
    e.newMode = mode;
    return e;
  }
};

//----------------------------------------------------------------------------//
// Hardware Configuration
//----------------------------------------------------------------------------//

const int LED_PIN = 13;    // Built-in LED
const int BUTTON_PIN = 2;  // Button with pull-up

//----------------------------------------------------------------------------//
// Pure Transition Function δ: Q × Σ → Q
//----------------------------------------------------------------------------//

AppState transitionFunction(const AppState& state, const Input& input) {
  AppState newState = state;
  newState.lastUpdate = millis();
  
  switch (input.type) {
    case INPUT_BUTTON_PRESSED:
      // Cycle through LED modes: OFF -> ON -> SLOW_BLINK -> FAST_BLINK -> OFF
      switch (state.mode) {
        case LED_OFF:
          newState.mode = LED_ON;
          break;
        case LED_ON:
          newState.mode = LED_BLINKING_SLOW;
          break;
        case LED_BLINKING_SLOW:
          newState.mode = LED_BLINKING_FAST;
          break;
        case LED_BLINKING_FAST:
          newState.mode = LED_OFF;
          break;
      }
      newState.blinkCounter = 0; // Reset blink counter on mode change
      break;
      
    case INPUT_TICK:
      // Increment blink counter for timing
      newState.blinkCounter++;
      break;
      
    default:
      break;
  }
  
  return newState;
}

//----------------------------------------------------------------------------//
// Pure Output Function λ: Q → Γ
//----------------------------------------------------------------------------//

Output outputFunction(const AppState& state) {
  // Moore property: effects depend only on current state
  switch (state.mode) {
    case LED_OFF:
      return Output::ledOff();
      
    case LED_ON:
      return Output::ledOn();
      
    case LED_BLINKING_SLOW:
      return Output::ledBlinkSlow(state.blinkCounter);
      
    case LED_BLINKING_FAST:
      return Output::ledBlinkFast(state.blinkCounter);
  }
  
  return Output::none();
}

//----------------------------------------------------------------------------//
// Effect Execution (I/O happens here)
//----------------------------------------------------------------------------//

void executeEffect(const Output& output) {
  switch (output.type) {
    case EFFECT_LED_OFF:
      digitalWrite(LED_PIN, LOW);
      break;
      
    case EFFECT_LED_ON:
      digitalWrite(LED_PIN, HIGH);
      break;
      
    case EFFECT_LED_BLINK_SLOW:
    case EFFECT_LED_BLINK_FAST:
      digitalWrite(LED_PIN, output.blinkState);
      break;
      
    case EFFECT_LOG_STATE_CHANGE:
      Serial.print("LED mode changed to: ");
      switch (output.newMode) {
        case LED_OFF: Serial.println("OFF"); break;
        case LED_ON: Serial.println("ON"); break;
        case LED_BLINKING_SLOW: Serial.println("BLINKING_SLOW"); break;
        case LED_BLINKING_FAST: Serial.println("BLINKING_FAST"); break;
      }
      break;
      
    case EFFECT_NONE:
    default:
      // No effect to execute
      break;
  }
}

//----------------------------------------------------------------------------//
// State Observers
//----------------------------------------------------------------------------//

void observeModeChanges(const AppState& oldState, const AppState& newState) {
  if (oldState.mode != newState.mode) {
    Serial.print("Observer: Mode transition ");
    Serial.print(oldState.mode);
    Serial.print(" -> ");
    Serial.println(newState.mode);
    
    // Generate a logging effect (this is a bit awkward - could be improved)
    Output logOutput = Output::logStateChange(newState.mode);
    executeEffect(logOutput);
  }
}

//----------------------------------------------------------------------------//
// Global Objects
//----------------------------------------------------------------------------//

MooreMachine<AppState, Input, Output> machine(transitionFunction, AppState());
Timer tickTimer(100);        // 10Hz tick rate
Button ledButton(BUTTON_PIN); // Button on pin 2

//----------------------------------------------------------------------------//
// Setup & Loop
//----------------------------------------------------------------------------//

void setup() {
  // Initialize hardware
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize serial
  Serial.begin(115200);
  delay(1000);
  Serial.println("Simple LED Controller - MooreArduino Demo");
  Serial.println("Press button to cycle through LED modes: OFF -> ON -> SLOW -> FAST -> OFF");
  
  // Set up Moore machine
  machine.addStateObserver(observeModeChanges);
  machine.setOutputFunction(outputFunction);
  
  // Start tick timer
  tickTimer.start();
  
  Serial.println("Ready! Current mode: OFF");
}

void loop() {
  // 1. Get current effect from Moore machine λ: Q → Γ
  Output effect = machine.getCurrentOutput();
  
  // 2. Execute effect in main loop (handle I/O)
  executeEffect(effect);
  
  // 3. Gather inputs from environment
  Input input = Input::none();
  
  if (ledButton.wasPressed()) {
    input = Input::buttonPressed();
  } else if (tickTimer.expired()) {
    tickTimer.restart();
    input = Input::tick();
  }
  
  // 4. Step the machine with new input δ: Q × Σ → Q
  if (input.type != INPUT_NONE) {
    machine.step(input);
  }
  
  delay(10); // Small delay to prevent overwhelming the system
}
