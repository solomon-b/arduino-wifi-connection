/*
 * Arduino Giga R1 WiFi Connection Manager
 * Moore Finite State Machine Implementation
 * 
 * This program implements a WiFi connection manager using a Moore machine,
 * providing predictable state management for embedded systems.
 * 
 * Moore Machine M = (Q, Σ, δ, λ, q₀) where:
 * - Q: State space {INITIALIZING, CONNECTING, CONNECTED, DISCONNECTED, ENTERING_CREDENTIALS}
 * - Σ: Input alphabet {RETRY_CONNECTION, REQUEST_CREDENTIALS, CREDENTIALS_ENTERED, WIFI_CONNECTED, etc.}
 * - δ: Transition function (implemented as `transitionFunction`)
 * - λ: Output function (implemented as `outputFunction` - generates effects)
 * - q₀: Initial state (INITIALIZING)
 * 
 * Key Properties:
 * - Outputs depend only on current state (Moore property)
 * - State transitions are pure functions with no side effects
 * - I/O operations are handled by effect execution based on state
 * - Deterministic: same input + state always produces same next state
 * 
 * Architecture:
 * 1. Input Processing: Convert events (user input, hardware) to formal inputs
 * 2. State Transition: Apply δ(currentState, input) → nextState
 * 3. Output Generation: λ(state) produces effects (LEDs, serial, WiFi operations)
 * 4. Output Execution: Handle all I/O operations based on effects
 * 5. State Observation: React to state changes for UI updates
 * 
 * Hardware:
 * - Arduino Giga R1 WiFi board
 * - Power LED on Digital Pin 2 (always on when powered)
 * - WiFi LED on Digital Pin 3 (indicates WiFi connection status)
 * - Serial connection at 115200 baud for user interface
 * 
 * Persistent Storage:
 * - WiFi credentials stored in KVStore (key-value storage in flash memory)
 * - Survives power cycles and board resets
 * 
 * User Interface:
 * - Serial monitor for credential input and status display
 * - Press 'c' to change WiFi credentials
 * - Press 'r' to retry connection when disconnected
 * 
 * State Transition Diagram:
 * INITIALIZING → CONNECTING → CONNECTED ⟷ DISCONNECTED
 *                     ↓           ↗
 *              ENTERING_CREDENTIALS
 */

// Arduino WiFi library for managing wireless connections
#include <WiFi.h>
// Key-Value store API for persistent credential storage in flash memory
#include "kvstore_global_api.h"
// Mbed error handling definitions
#include <mbed_error.h>
// MooreArduino library for Moore machine implementation and utilities
#include <MooreArduino.h>

// Local modules
#include "WiFiTypes.h"
#include "WiFiCredentials.h"
#include "WiFiConnection.h"
#include "WiFiUI.h"
#include "WiFiStateMachine.h"

using namespace MooreArduino;

//----------------------------------------------------------------------------//
// Debug Configuration
//----------------------------------------------------------------------------//

// Toggle debug output at compile time
// Set to 1 to enable verbose debug logging, 0 for production builds
// When disabled, all DEBUG_PRINT/DEBUG_PRINTLN calls compile to nothing (zero overhead)
#define DEBUG_ENABLED 1

// Preprocessor macros that conditionally compile debug statements
// These are C++ preprocessor directives - they're replaced at compile time
#if DEBUG_ENABLED
  #define DEBUG_PRINT(x) Serial.print(x)     // Print without newline
  #define DEBUG_PRINTLN(x) Serial.println(x) // Print with newline
#else
  #define DEBUG_PRINT(x)    // Compiles to nothing
  #define DEBUG_PRINTLN(x)  // Compiles to nothing
#endif

//----------------------------------------------------------------------------//
// Hardware Configuration
//----------------------------------------------------------------------------//

// Arduino pin assignments for LED indicators
// Digital pins can be HIGH (3.3V) or LOW (0V)
const int power_led_pin = 2;  // Power indicator LED (always on when board is powered)
const int wifi_led_pin = 3;   // WiFi status LED (on when connected, blinks when connecting)

//----------------------------------------------------------------------------//
// Global State Management
//----------------------------------------------------------------------------//

// Moore machine instance with transition function and initial state
MooreMachine<AppState, Input, Output> g_machine(transitionFunction, AppState());

// Global utilities
Timer g_tickTimer(100);  // 100ms tick rate (10Hz)
Button g_resetButton(4); // Optional reset button on pin 4

//----------------------------------------------------------------------------//
// Arduino Setup Function
//----------------------------------------------------------------------------//

void setup() {
  // Configure LED pins as outputs
  pinMode(wifi_led_pin, OUTPUT);    // WiFi status LED
  pinMode(power_led_pin, OUTPUT);   // Power indicator LED
  digitalWrite(power_led_pin, HIGH); // Turn on power LED immediately
  digitalWrite(wifi_led_pin, LOW);   // WiFi LED starts off

  // Initialize serial communication at 115200 baud
  Serial.begin(115200);
  while (!Serial);  // Wait for serial port to connect (USB serial)

  delay(1000);  // Brief pause for system stabilization

  // Verify WiFi hardware is present
  Serial.println("Checking WiFi module...");
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("ERROR: WiFi module not detected!");
    // Infinite error loop with fast blinking WiFi LED
    while (true) {
      digitalWrite(wifi_led_pin, HIGH);
      delay(100);
      digitalWrite(wifi_led_pin, LOW);
      delay(100);
    }
  }
  
  // Log current WiFi module status for debugging
  Serial.print("WiFi module status: ");
  Serial.println(WiFi.status());
  
  // Display firmware version for diagnostics
  Serial.print("WiFi firmware: ");
  Serial.println(WiFi.firmwareVersion());

  // Set up store observers for reactive UI updates
  g_machine.addStateObserver(observeConnectedState);
  g_machine.addStateObserver(observeDisconnectedState);
  g_machine.addStateObserver(observeCredentialChanges);
  
  // Set up output function
  g_machine.setOutputFunction(outputFunction);
  
  // Start tick timer
  g_tickTimer.start();
  
  // Attempt to load saved WiFi credentials from flash memory
  Credentials loadedCreds;
  if (!loadCredentials(&loadedCreds)) {
    Serial.println("No stored credentials.");
    // No credentials found - start credential entry process
    g_machine.step(Input::requestCredentials());
  } else {
    // Credentials found - inject them into state and attempt to connect
    g_machine.step(Input::credentialsEntered(loadedCreds));
  }
}

//----------------------------------------------------------------------------//
// Arduino Main Loop
//----------------------------------------------------------------------------//

void loop() {
  const AppState& state = g_machine.getState();
  
  // Debug logging of current state (only when DEBUG_ENABLED=1)
  DEBUG_PRINT("DEBUG: Loop iteration, mode=");
  DEBUG_PRINT(state.mode);
  DEBUG_PRINT(", shouldReconnect=");
  DEBUG_PRINTLN(state.shouldReconnect);
  
  // 1. Get current effect from Moore machine λ: Q → Γ
  Output effect = g_machine.getCurrentOutput();
  
  // 2. Execute effect (handle I/O) and get follow-up input
  Input followUpInput = executeEffect(effect);
  
  // 3. Process follow-up input if needed
  if (followUpInput.type != INPUT_NONE) {
    DEBUG_PRINT("DEBUG: Follow-up input type=");
    DEBUG_PRINTLN(followUpInput.type);
    g_machine.step(followUpInput);
    return; // Return early to avoid processing user input in same loop
  }
  
  // 4. Read events from environment (user input, hardware status)
  Input input = readEvents();
  
  if (input.type != INPUT_NONE) {
    DEBUG_PRINT("DEBUG: Input type=");
    DEBUG_PRINTLN(input.type);
    
    // Special handling for credential entry (only blocking operation)
    if (input.type == INPUT_REQUEST_CREDENTIALS) {
      g_machine.step(input);  // First, change to credential entry mode
      
      // Blocking credential prompt (breaks Moore pattern but necessary for UX)
      Credentials newCreds;
      if (promptForCredentialsBlocking(&newCreds)) {
        DEBUG_PRINTLN("DEBUG: About to process credentialsEntered input");
        g_machine.step(Input::credentialsEntered(newCreds));  // Process entered credentials
        DEBUG_PRINTLN("DEBUG: After processing credentialsEntered input");
        // Continue loop - don't return early
      } else {
        // Credential entry failed - revert to previous state
        g_machine.step(Input::tick());  // Tick will update mode based on WiFi status
      }
    } else {
      // Normal case - process input through Moore machine
      g_machine.step(input);
    }
  }
  
  delay(10);  // Small delay to prevent overwhelming the system
}
