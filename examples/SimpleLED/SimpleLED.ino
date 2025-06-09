/*
 * Simple LED Controller using MooreArduino Library
 * 
 * This example demonstrates basic Moore machine patterns with the MooreArduino library.
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
 * - δ (transition): Button press cycles modes, tick advances blink timing
 * - λ (output): LED control and serial logging based on current state
 */

#include <MooreArduino.h>

using namespace MooreArduino;

//----------------------------------------------------------------------------//
// State Space Q & Input Alphabet Σ
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
// Output Function λ: Q → Γ
//----------------------------------------------------------------------------//

Input outputFunction(const AppState& oldState, const AppState& newState) {
  // Moore property: LED output depends only on current state
  switch (newState.mode) {
    case LED_OFF:
      digitalWrite(LED_PIN, LOW);
      break;
      
    case LED_ON:
      digitalWrite(LED_PIN, HIGH);
      break;
      
    case LED_BLINKING_SLOW:
      // Blink every 50 ticks (500ms at 10Hz)
      digitalWrite(LED_PIN, (newState.blinkCounter / 5) % 2);
      break;
      
    case LED_BLINKING_FAST:
      // Blink every 10 ticks (100ms at 10Hz)
      digitalWrite(LED_PIN, newState.blinkCounter % 2);
      break;
  }
  
  // Print state changes to serial
  if (newState.mode != oldState.mode) {
    Serial.print("LED mode changed to: ");
    switch (newState.mode) {
      case LED_OFF: Serial.println("OFF"); break;
      case LED_ON: Serial.println("ON"); break;
      case LED_BLINKING_SLOW: Serial.println("BLINKING_SLOW"); break;
      case LED_BLINKING_FAST: Serial.println("BLINKING_FAST"); break;
    }
  }
  
  return Input::none();
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
  }
}

//----------------------------------------------------------------------------//
// Global Objects
//----------------------------------------------------------------------------//

MooreMachine<AppState, Input> machine(transitionFunction, AppState());
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
  // Check for button press
  if (ledButton.wasPressed()) {
    machine.step(Input::buttonPressed());
  }
  
  // Send tick input at regular intervals
  if (tickTimer.expired()) {
    tickTimer.restart();
    machine.step(Input::tick());
  }
  
  delay(10); // Small delay to prevent overwhelming the system
}