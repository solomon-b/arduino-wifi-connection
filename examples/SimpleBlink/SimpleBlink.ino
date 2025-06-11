/*
 * SimpleBlink - Moore Machine LED Blinker
 * 
 * This is the simplest possible Moore machine example to validate
 * the MooreArduino library functionality.
 * 
 * Moore Machine M = (Q, Σ, δ, λ, q₀) where:
 * - Q: {LED_ON, LED_OFF}
 * - Σ: {TICK}
 * - δ: LED_ON + TICK → LED_OFF, LED_OFF + TICK → LED_ON
 * - λ: LED_ON → digitalWrite(HIGH), LED_OFF → digitalWrite(LOW)
 * - q₀: LED_OFF
 */

#include <MooreArduino.h>
using namespace MooreArduino;

// Hardware pin
const int LED_PIN = 2;

// State space Q
enum BlinkState {
  LED_OFF,
  LED_ON
};

// Input alphabet Σ  
enum BlinkInput {
  INPUT_NONE,
  INPUT_TICK
};

// Output alphabet Γ
enum BlinkOutput {
  OUTPUT_NONE,
  OUTPUT_SET_LED
};

// Input structure
struct Input {
  BlinkInput type;
  
  Input() : type(INPUT_NONE) {}
  
  static Input none() {
    Input i;
    i.type = INPUT_NONE;
    return i;
  }
  
  static Input tick() {
    Input i;
    i.type = INPUT_TICK;
    return i;
  }
};

// Output structure
struct Output {
  BlinkOutput type;
  bool ledState;
  
  Output() : type(OUTPUT_NONE), ledState(false) {}
  
  static Output none() {
    Output o;
    o.type = OUTPUT_NONE;
    return o;
  }
  
  static Output setLED(bool state) {
    Output o;
    o.type = OUTPUT_SET_LED;
    o.ledState = state;
    return o;
  }
};

// Pure transition function δ: Q × Σ → Q
BlinkState transitionFunction(const BlinkState& state, const Input& input) {
  if (input.type == INPUT_TICK) {
    // Toggle state on each tick
    return (state == LED_OFF) ? LED_ON : LED_OFF;
  }
  // No change for other inputs
  return state;
}

// Pure output function λ: Q → Γ
Output outputFunction(const BlinkState& state) {
  switch (state) {
    case LED_ON:
      return Output::setLED(true);
    case LED_OFF:
      return Output::setLED(false);
    default:
      return Output::none();
  }
}

// Execute outputs (handle I/O)
void executeOutput(const Output& output) {
  switch (output.type) {
    case OUTPUT_SET_LED:
      digitalWrite(LED_PIN, output.ledState ? HIGH : LOW);
      Serial.print("LED: ");
      Serial.println(output.ledState ? "ON" : "OFF");
      break;
    case OUTPUT_NONE:
    default:
      // No action
      break;
  }
}

// Global Moore machine
MooreMachine<BlinkState, Input, Output> machine(transitionFunction, LED_OFF);

// Timer for generating tick inputs
Timer tickTimer(1000); // 1 second

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  
  // Set output function
  machine.setOutputFunction(outputFunction);
  
  // Start timer
  tickTimer.start();
  
  Serial.println("SimpleBlink Moore Machine Started");
  Serial.println("LED should blink every second");
}

void loop() {
  // Generate tick input when timer expires
  if (tickTimer.expired()) {
    tickTimer.restart();
    machine.step(Input::tick());
  }
  
  // Execute current output
  Output currentOutput = machine.getCurrentOutput();
  executeOutput(currentOutput);
  
  delay(10); // Small delay to prevent overwhelming
}
