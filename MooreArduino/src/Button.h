#ifndef REDUX_BUTTON_H
#define REDUX_BUTTON_H

#include <Arduino.h>

namespace MooreArduino {

/**
 * Debounced button for Redux applications
 * 
 * Handles button debouncing automatically and provides clean press detection.
 * 
 * Usage:
 *   Button powerButton(2);  // Button on pin 2
 *   
 *   if (powerButton.wasPressed()) {
 *     // Button was just pressed
 *     store.dispatch(Action::powerToggle());
 *   }
 */
class Button {
private:
  int pin;
  bool lastState;
  bool currentState;
  unsigned long lastChangeTime;
  unsigned long debounceDelay;

public:
  static const unsigned long DEFAULT_DEBOUNCE_DELAY = 50; // ms

  /**
   * Create a button on the specified pin
   * Sets up INPUT_PULLUP mode automatically
   */
  Button(int pinNumber, unsigned long debounceMs = DEFAULT_DEBOUNCE_DELAY) 
    : pin(pinNumber), lastState(HIGH), currentState(HIGH), 
      lastChangeTime(0), debounceDelay(debounceMs) {
    pinMode(pin, INPUT_PULLUP);
  }

  /**
   * Update button state and return true if button was just pressed
   * Call this once per loop iteration
   */
  bool wasPressed() {
    return update() && isPressed();
  }

  /**
   * Check if button is currently pressed (after debouncing)
   */
  bool isPressed() const {
    return currentState == LOW;
  }

  /**
   * Update the button state (called automatically by wasPressed)
   * Returns true if state changed
   */
  bool update() {
    bool reading = digitalRead(pin);
    
    // If state changed, reset debounce timer
    if (reading != lastState) {
      lastChangeTime = millis();
    }
    
    // If enough time has passed, accept the new state
    if (millis() - lastChangeTime > debounceDelay) {
      if (reading != currentState) {
        currentState = reading;
        lastState = reading;
        return true;  // State changed
      }
    }
    
    lastState = reading;
    return false;  // No change
  }

  /**
   * Set debounce delay in milliseconds
   */
  void setDebounceDelay(unsigned long ms) {
    debounceDelay = ms;
  }

  /**
   * Get current debounce delay
   */
  unsigned long getDebounceDelay() const {
    return debounceDelay;
  }

  /**
   * Get the pin number this button is on
   */
  int getPin() const {
    return pin;
  }
};

} // namespace MooreArduino

#endif // REDUX_BUTTON_H