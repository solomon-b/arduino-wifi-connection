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
 * - λ: Output function (implemented as `outputFunction` - generates follow-up inputs)
 * - q₀: Initial state (INITIALIZING)
 * 
 * Key Properties:
 * - Outputs depend only on current state (Moore property)
 * - State transitions are pure functions with no side effects
 * - I/O operations are handled by output functions based on state
 * - Deterministic: same input + state always produces same next state
 * 
 * Architecture:
 * 1. Input Processing: Convert events (user input, hardware) to formal inputs
 * 2. State Transition: Apply δ(currentState, input) → nextState
 * 3. Output Generation: λ(state) produces outputs (LEDs, serial, WiFi operations)
 * 4. State Observation: React to state changes for UI updates
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
// Hardware Constants
//----------------------------------------------------------------------------//

// Key names for persistent storage in KVStore (flash memory)
// These strings identify our stored WiFi credentials
const char* KEY_SSID = "wifi_ssid";  // Key for storing WiFi network name
const char* KEY_PASS = "wifi_pass";  // Key for storing WiFi password

// Arduino pin assignments for LED indicators
// Digital pins can be HIGH (3.3V) or LOW (0V)
const int power_led_pin = 2;  // Power indicator LED (always on when board is powered)
const int wifi_led_pin = 3;   // WiFi status LED (on when connected, blinks when connecting)

//----------------------------------------------------------------------------//
// Type Definitions (Redux Architecture Data Structures)
//----------------------------------------------------------------------------//

/*
 * In Redux-style architecture, we model our application as a state machine.
 * These types define the "vocabulary" of our system - what states exist
 * and what actions can change those states.
 */

/*
 * InputType: The input alphabet Σ of our Moore machine
 * 
 * In Moore machine theory, inputs are symbols from a finite alphabet that trigger
 * state transitions. These represent events that can occur in our system:
 * user inputs, timer events, hardware state changes, etc.
 * 
 * Each input type represents a distinct stimulus that can cause the machine
 * to transition from one state to another via the transition function δ.
 */
enum InputType {
  INPUT_NONE,                     // No input (used as default/placeholder)
  INPUT_RETRY_CONNECTION,         // User pressed 'r' to retry WiFi connection
  INPUT_REQUEST_CREDENTIALS,      // User pressed 'c' to enter new WiFi credentials
  INPUT_CREDENTIALS_ENTERED,      // User finished entering SSID and password
  INPUT_CONNECTION_STARTED,       // WiFi.begin() was called, reset shouldReconnect flag
  INPUT_WIFI_CONNECTED,           // Hardware detected WiFi connection established
  INPUT_WIFI_DISCONNECTED,        // Hardware detected WiFi connection lost
  INPUT_TICK                      // Timer event - check for state changes
};

/*
 * Credentials: WiFi network authentication data
 * 
 * In C++, 'struct' is like a class but with public members by default.
 * This struct holds WiFi network name (SSID) and password.
 * 
 * Key C++ concepts:
 * - char arrays: Fixed-size string buffers (64 characters max)
 * - const methods: Promise not to modify the object's data
 * - Member functions: Methods that belong to this struct
 */
struct Credentials {
  char ssid[64];  // WiFi network name (fixed-size character array)
  char pass[64];  // WiFi password (fixed-size character array)
  
  // Helper method to check if credentials are empty
  // 'const' means this method won't modify the struct's data
  bool isEmpty() const {
    return ssid[0] == '\0';  // '\0' is the null terminator (end of string)
  }
  
  // Helper method to validate credential length
  // strlen() counts characters until it hits the null terminator
  bool isValid() const {
    return strlen(ssid) > 0 && strlen(ssid) < 64 && 
           strlen(pass) > 0 && strlen(pass) < 64;
  }
};

/*
 * AppMode: The state space Q of our Moore machine
 * 
 * This enum represents the finite set of states our Moore machine can be in.
 * In Moore machine theory, the output (LED patterns, UI messages) depends
 * only on the current state, not on the input that caused the transition.
 * 
 * State space Q = {INITIALIZING, CONNECTING, CONNECTED, DISCONNECTED, ENTERING_CREDENTIALS}
 * 
 * State transitions via δ: Q × Σ → Q:
 * INITIALIZING → CONNECTING → CONNECTED ⇄ DISCONNECTED
 *                   ↓           ↗
 *            ENTERING_CREDENTIALS
 */
enum AppMode {
  MODE_INITIALIZING,         // Starting up, checking for stored credentials
  MODE_CONNECTING,           // Attempting to connect to WiFi network
  MODE_CONNECTED,            // Successfully connected to WiFi
  MODE_DISCONNECTED,         // Not connected (initial state or lost connection)
  MODE_ENTERING_CREDENTIALS  // User is typing SSID/password via Serial
};

/*
 * AppState: Current state q ∈ Q of the Moore machine
 * 
 * This struct represents the complete state of our Moore machine at any given time.
 * In Moore machine theory, this is the current configuration that determines:
 * 1. What outputs are produced (λ(q) → outputs)
 * 2. What transitions are possible given an input (δ(q, σ) → q')
 * 
 * The state contains all information needed to:
 * - Determine current outputs (LED states, UI messages)
 * - Process the next input symbol
 * - Maintain system consistency across state transitions
 * 
 * Key C++ concepts:
 * - Constructor: Special method called when creating an object
 * - Initialization list: Efficient way to set member values in constructor
 * - unsigned long: 32-bit positive integer (for timestamps)
 * - Member initialization: Setting values when the object is created
 */
struct AppState {
  Credentials credentials;      // Current WiFi network credentials
  AppMode mode;                // What the application is currently doing
  int wifiStatus;              // Last known WiFi hardware status
  unsigned long lastUpdate;    // Timestamp of last state change (milliseconds)
  bool credentialsChanged;     // Flag: need to save credentials to flash
  bool shouldReconnect;        // Flag: need to call WiFi.begin()
  
  // Constructor: Called when creating a new AppState
  // The colon starts an "initialization list" - efficient way to set member values
  AppState() : mode(MODE_INITIALIZING),           // Start in initializing mode
               wifiStatus(WL_IDLE_STATUS),        // WiFi not started yet
               lastUpdate(0),                     // No timestamp yet
               credentialsChanged(false),         // No changes to save
               shouldReconnect(false) {           // No connection needed yet
    // Set credential strings to empty (null-terminated)
    credentials.ssid[0] = '\0';  // Empty string
    credentials.pass[0] = '\0';  // Empty string
  }
};

/*
 * Input: Symbols from input alphabet Σ
 * 
 * Input symbols carry information about events that occurred. They are formal
 * symbols from our input alphabet Σ that are fed to the transition function δ.
 * The transition function δ(q, σ) uses current state q and input symbol σ
 * to determine the next state q'.
 * 
 * Key C++ concepts:
 * - static methods: Class methods that don't need an object instance
 * - Factory pattern: Static methods that create and return objects
 * - const reference: Efficient way to pass objects without copying
 * - Ternary operator: condition ? value_if_true : value_if_false
 */
struct Input {
  InputType type;                 // Which input symbol this is
  Credentials newCredentials;     // New credentials (if INPUT_CREDENTIALS_ENTERED)
  int wifiStatus;                // WiFi status code (if INPUT_WIFI_*)
  
  // Default constructor
  Input() : type(INPUT_NONE), wifiStatus(0) {
    newCredentials.ssid[0] = '\0';
    newCredentials.pass[0] = '\0';
  }
  
  // Factory methods: Static functions that create Input symbols
  // These are like constructors but more explicit about what they create
  
  static Input none() {
    Input i;                      // Create empty Input on the stack
    i.type = INPUT_NONE;
    return i;                     // Return by value (copied)
  }
  
  static Input retryConnection() {
    Input i;
    i.type = INPUT_RETRY_CONNECTION;
    return i;
  }
  
  static Input requestCredentials() {
    Input i;
    i.type = INPUT_REQUEST_CREDENTIALS;
    return i;
  }
  
  // const Credentials& means "reference to Credentials that won't be modified"
  // This avoids copying the 128-byte Credentials struct
  static Input credentialsEntered(const Credentials& creds) {
    Input i;
    i.type = INPUT_CREDENTIALS_ENTERED;
    i.newCredentials = creds;     // This DOES copy the credentials
    return i;
  }
  
  static Input connectionStarted() {
    Input i;
    i.type = INPUT_CONNECTION_STARTED;
    return i;
  }
  
  // Ternary operator: condition ? value_if_true : value_if_false
  static Input wifiStatusChanged(int status) {
    Input i;
    i.type = (status == WL_CONNECTED) ? INPUT_WIFI_CONNECTED : INPUT_WIFI_DISCONNECTED;
    i.wifiStatus = status;
    return i;
  }
  
  static Input tick() {
    Input i;
    i.type = INPUT_TICK;
    return i;
  }
};

//----------------------------------------------------------------------------//
// Global State (Single Source of Truth)
//----------------------------------------------------------------------------//

/*
 * Global utilities (machine declared later after transition function definition)
 * 
 * Using the ReduxArduino library for state management.
 */
Timer g_tickTimer(100);  // 100ms tick rate (10Hz)
Button g_resetButton(4); // Optional reset button on pin 4

//----------------------------------------------------------------------------//
// Debug Helper Functions  
//----------------------------------------------------------------------------//

// Helper function to convert enum values to human-readable strings
// Only compiled when DEBUG_ENABLED=1 to save memory in production
#if DEBUG_ENABLED
const char* getModeString(AppMode mode) {
  switch (mode) {
    case MODE_INITIALIZING: return "INITIALIZING";            // Startup phase
    case MODE_CONNECTING: return "CONNECTING";                // WiFi connection in progress
    case MODE_CONNECTED: return "CONNECTED";                  // Successfully connected
    case MODE_DISCONNECTED: return "DISCONNECTED";            // Not connected to WiFi
    case MODE_ENTERING_CREDENTIALS: return "ENTERING_CREDENTIALS"; // User typing credentials
    default: return "UNKNOWN";                               // Invalid mode (shouldn't happen)
  }
}
#endif

//----------------------------------------------------------------------------//
// Forward Declarations
//----------------------------------------------------------------------------//

// Declare functions that are used before they're defined
AppState transitionFunction(const AppState& state, const Input& input);
Input outputFunction(const AppState& oldState, const AppState& newState);
void observeConnectedState(const AppState& oldState, const AppState& newState);
void observeDisconnectedState(const AppState& oldState, const AppState& newState);
void observeCredentialChanges(const AppState& oldState, const AppState& newState);
bool loadCredentials(Credentials* creds);

//----------------------------------------------------------------------------//
// Moore Machine Declaration (after transition function definition)
//----------------------------------------------------------------------------//

// Create the Moore machine with transition function and initial state
MooreMachine<AppState, Input> g_machine(transitionFunction, AppState());

//----------------------------------------------------------------------------//

// Arduino setup function - called once at startup
void setup() {
  // Configure LED pins as outputs
  pinMode(wifi_led_pin, OUTPUT);    // WiFi status LED
  pinMode(power_led_pin, OUTPUT);   // Power indicator LED
  digitalWrite(power_led_pin, HIGH); // Turn on power LED immediately
  digitalWrite(wifi_led_pin, LOW);   // WiFi LED starts off

  // Initialize serial communication at 9600 baud
  Serial.begin(9600);
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

// Persist WiFi credentials to flash memory using KVStore
void saveCredentials(const Credentials* creds) {
  // Get pointers to credential strings for convenience
  const char* s = creds->ssid;
  const char* p = creds->pass;
  
  // Calculate size including null terminator (+1)
  size_t ssid_size = strlen(s) + 1;
  // Store SSID in key-value store (last param 0 = no flags)
  int set_ssid_result = kv_set(KEY_SSID, s, ssid_size, 0);

  // Check for storage errors - halt on failure (critical error)
  if (set_ssid_result != MBED_SUCCESS) {
    Serial.print("'kv_set(KEY_SSID, s, ssid_size, 0)' failed with error code ");
    Serial.print(set_ssid_result);
    while (true) {}  // Infinite loop - unrecoverable error
  }

  // Store password using same pattern
  size_t pass_size = strlen(p) + 1;
  int set_pass_result = kv_set(KEY_PASS, p, pass_size, 0);

  // Check for storage errors
  if (set_pass_result != MBED_SUCCESS) {
    Serial.print("'kv_set(KEY_PASS, p, pass_size, 0)' failed with error code ");
    Serial.print(set_pass_result);
    while (true) {}  // Infinite loop - unrecoverable error
  }
}

// Load WiFi credentials from flash memory - returns true if found
bool loadCredentials(Credentials* creds) {
  // KVStore info structures to get size information
  kv_info_t ssid_buffer;
  kv_info_t pass_buffer;

  // Get metadata about stored items (size, etc.)
  int get_ssid_result = kv_get_info(KEY_SSID, &ssid_buffer);
  int get_pass_result = kv_get_info(KEY_PASS, &pass_buffer);

  // Check if either credential is missing (normal case for first run)
  if (get_ssid_result == MBED_ERROR_ITEM_NOT_FOUND || get_pass_result == MBED_ERROR_ITEM_NOT_FOUND) {
    return false;  // No credentials stored yet
  } else if (get_ssid_result != MBED_SUCCESS ) {
    // Unexpected error accessing SSID
    Serial.print("kv_get_info failed for KEY_SSID with ");
    Serial.println(get_ssid_result);
    while (true) {}  // Critical error - halt
  } else if (get_pass_result != MBED_SUCCESS) {
    // Unexpected error accessing password
    Serial.print("kv_get_info failed for KEY_PASS with ");
    Serial.println(get_pass_result);
    while (true) {}  // Critical error - halt
  }

  // Clear SSID buffer and read stored value
  memset(creds->ssid, 0, sizeof(creds->ssid));
  int read_ssid_result = kv_get(KEY_SSID, creds->ssid, ssid_buffer.size, nullptr);

  // Check for read errors
  if (read_ssid_result != MBED_SUCCESS) {
    Serial.print("'kv_get(KEY_SSID, ssid, sizeof(ssid), nullptr);' failed with error code ");
    Serial.println(read_ssid_result);
    while (true) {}  // Critical error - halt
  }
  
  // Clear password buffer and read stored value
  memset(creds->pass, 0, sizeof(creds->pass));
  int read_pass_result = kv_get(KEY_PASS, creds->pass, pass_buffer.size, nullptr);

  // Check for read errors
  if (read_pass_result != MBED_SUCCESS) {
    Serial.print("'kv_get(KEY_PASS, pass, sizeof(ssid), nullptr);' failed with error code ");
    Serial.println(read_pass_result);
    while (true) {}  // Critical error - halt
  }

  // Return true if both operations succeeded
  return (get_ssid_result == MBED_SUCCESS && get_pass_result == MBED_SUCCESS);
}

// Clear any pending serial input to prevent stale data
void flushSerialInput() {
  while (Serial.available()) Serial.read();  // Read and discard all pending bytes
}

// Blocking function to prompt user for WiFi credentials via Serial
// Returns true if valid credentials entered, false on validation failure
bool promptForCredentialsBlocking(Credentials* creds) {
  flushSerialInput();  // Clear any stale input

  // Prompt for and read SSID
  Serial.println("Enter SSID:");
  while (!Serial.available());  // Block until user types something
  String ssid_str = Serial.readStringUntil('\n');  // Read until newline
  ssid_str.trim();  // Remove leading/trailing whitespace

  // Validate SSID length (must be 1-63 chars)
  if (!isValidCredentialLength(ssid_str)) {
    Serial.println("Invalid SSID length. Aborting.");
    return false;  // Validation failed
  }

  // Convert Arduino String to C-style char array
  ssid_str.toCharArray(creds->ssid, sizeof(creds->ssid));
  flushSerialInput();  // Clear any remaining input

  // Prompt for and read password using same pattern
  Serial.println("Enter Password:");
  while (!Serial.available());  // Block until user types something
  String pass_str = Serial.readStringUntil('\n');  // Read until newline
  pass_str.trim();  // Remove leading/trailing whitespace

  // Validate password length
  if (!isValidCredentialLength(pass_str)) {
    Serial.println("Invalid password length. Aborting.");
    return false;  // Validation failed
  }

  // Convert to C-style char array
  pass_str.toCharArray(creds->pass, sizeof(creds->pass));
  return true;  // Success
}

// Non-blocking credential prompt (currently unused - kept for reference)
// This version saves credentials immediately without returning them
void promptForCredentials(Credentials* creds) {
  flushSerialInput();  // Clear stale input

  // Prompt for SSID
  Serial.println("Enter SSID:");
  while (!Serial.available());  // Wait for user input
  String ssid_str = Serial.readStringUntil('\n');  // Read line
  ssid_str.trim();  // Remove whitespace

  // Validate SSID length
  if (!isValidCredentialLength(ssid_str)) {
    Serial.println("Invalid SSID length. Not saving.");
    return;  // Early exit on validation failure
  }

  // Convert to fixed-size C-string buffer
  char ssid_buf[64] = {0};  // Zero-initialize
  ssid_str.toCharArray(ssid_buf, sizeof(ssid_buf));

  flushSerialInput();  // Clear input between prompts

  // Prompt for password using same pattern
  Serial.println("Enter Password:");
  while (!Serial.available());  // Wait for user input
  String pass_str = Serial.readStringUntil('\n');  // Read line
  pass_str.trim();  // Remove whitespace

  // Validate password length
  if (!isValidCredentialLength(pass_str)) {
    Serial.println("Invalid password length. Not saving.");
    return;  // Early exit on validation failure
  }

  // Convert to fixed-size C-string buffer
  char pass_buf[64] = {0};  // Zero-initialize
  pass_str.toCharArray(pass_buf, sizeof(pass_buf));

  // Copy validated strings to output structure
  strncpy(creds->ssid, ssid_buf, sizeof(creds->ssid));
  strncpy(creds->pass, pass_buf, sizeof(creds->pass));

  // Immediately save to persistent storage
  saveCredentials(creds);
}

// Initiate WiFi connection to specified network
// Performs network scan first to verify target exists
void connectWiFi(const Credentials* creds) {
  // Log connection attempt with SSID details
  Serial.print("Connecting to SSID: '");
  Serial.print(creds->ssid);
  Serial.print("' (length: ");
  Serial.print(strlen(creds->ssid));
  Serial.println(")");
  
  // Scan for available networks before connecting
  Serial.println("Scanning for networks...");
  Serial.println("This may take 10-15 seconds...");
  int numNetworks = WiFi.scanNetworks();  // Blocking call
  Serial.print("Scan completed. Found ");
  Serial.print(numNetworks);
  Serial.println(" networks:");
  
  // Handle case where no networks detected
  if (numNetworks == 0) {
    Serial.println("No networks found. Possible issues:");
    Serial.println("1. WiFi antenna not connected");
    Serial.println("2. WiFi module hardware problem"); 
    Serial.println("3. Distance from access point too far");
    Serial.println("4. WiFi module not properly initialized");
  }
  
  // Search scan results for target network
  bool networkFound = false;
  for (int i = 0; i < numNetworks; i++) {
    // Display each network: index, SSID, signal strength
    Serial.print(i);
    Serial.print(": ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" (");
    Serial.print(WiFi.RSSI(i));  // Received Signal Strength Indicator
    Serial.println(" dBm)");
    
    // Check if this is our target network (case-sensitive string compare)
    if (strcmp(WiFi.SSID(i), creds->ssid) == 0) {
      networkFound = true;
      Serial.println("  ^ Target network found!");
    }
  }
  
  // Abort connection if target network not found in scan
  if (!networkFound) {
    Serial.println("ERROR: Target network not found in scan!");
    digitalWrite(wifi_led_pin, LOW);  // Turn off WiFi LED
    return;  // Early exit
  }

  // Begin connection attempt (non-blocking)
  Serial.println("Starting WiFi connection...");
  WiFi.begin(creds->ssid, creds->pass);
  
  // Don't block here - let the FRP tick system handle status polling
}

//----------------------------------------------------------------------------//
// Pure Functions
//----------------------------------------------------------------------------//

// Parse single character user input into Input symbols (pure function)
// Only accepts input that's valid for current mode
Input parseUserInput(char input, AppMode currentMode) {
  switch (input) {
    case 'r':  // Retry connection
    case 'R':
      // Only allow retry when disconnected
      return (currentMode == MODE_DISCONNECTED) ? Input::retryConnection() : Input::none();
    case 'c':  // Change credentials
    case 'C':
      // Allow credential change from any mode
      return Input::requestCredentials();
    default:
      // Ignore unknown input
      return Input::none();
  }
}

//----------------------------------------------------------------------------//
// State Transition Function δ: Q × Σ → Q
//----------------------------------------------------------------------------//

// State transition function δ: Q × Σ → Q (pure function)
// This is the heart of the Moore machine - implements δ(q, σ) → q'
AppState transitionFunction(const AppState& state, const Input& input) {
  AppState newState = state;          // Copy current state
  newState.lastUpdate = millis();     // Update timestamp on every input
  
  switch (input.type) {
    case INPUT_NONE:
      // No-op input - return state unchanged
      return newState;
      
    case INPUT_REQUEST_CREDENTIALS:
      // User wants to enter new WiFi credentials
      newState.mode = MODE_ENTERING_CREDENTIALS;
      return newState;
      
    case INPUT_CREDENTIALS_ENTERED:
      // User finished entering credentials - prepare for connection
      newState.credentials = input.newCredentials;  // Store new credentials
      newState.credentialsChanged = true;           // Flag for persistence
      newState.shouldReconnect = true;              // Flag for connection attempt
      newState.mode = MODE_CONNECTING;              // Change to connecting state
      return newState;
      
    case INPUT_CONNECTION_STARTED:
      // WiFi.begin() was called - clear the reconnect flag
      newState.shouldReconnect = false;
      return newState;
      
    case INPUT_RETRY_CONNECTION:
      // User requested connection retry
      newState.shouldReconnect = true;   // Set flag for side effects
      newState.mode = MODE_CONNECTING;   // Change to connecting state
      return newState;
      
    case INPUT_WIFI_CONNECTED:
      // Hardware reports successful WiFi connection
      newState.mode = MODE_CONNECTED;           // Update mode
      newState.wifiStatus = input.wifiStatus;  // Store hardware status
      newState.shouldReconnect = false;        // Clear retry flag
      return newState;
      
    case INPUT_WIFI_DISCONNECTED:
      // Hardware reports WiFi connection lost
      newState.mode = MODE_DISCONNECTED;        // Update mode
      newState.wifiStatus = input.wifiStatus;  // Store hardware status
      return newState;
      
    case INPUT_TICK: {
      // Periodic status check - poll WiFi hardware
      int currentWifiStatus = WiFi.status();
      if (currentWifiStatus != newState.wifiStatus) {
        // WiFi status changed - update state accordingly
        newState.wifiStatus = currentWifiStatus;
        if (currentWifiStatus == WL_CONNECTED && newState.mode != MODE_CONNECTED) {
          newState.mode = MODE_CONNECTED;    // Hardware connected
        } else if (currentWifiStatus != WL_CONNECTED && newState.mode == MODE_CONNECTED) {
          newState.mode = MODE_DISCONNECTED; // Hardware disconnected
        }
      }
      return newState;
    }
      
    default:
      // Unknown input type - log and return unchanged state
      DEBUG_PRINTLN("Unknown input type in transition function");
      return newState;
  }
}


//----------------------------------------------------------------------------//
// Pure Helper Functions
//----------------------------------------------------------------------------//

// Read single character from serial input (pure function)
// Returns '\0' if no input available, otherwise returns first character
char readSingleChar() {
  if (!Serial.available()) return '\0';  // No input available
  char input = Serial.read();            // Read first byte
  flushSerialInput();                    // Discard remaining input
  return input;                          // Return the character
}

// Validate WiFi credential length (pure function)
// WiFi SSIDs and passwords must be 1-63 characters
bool isValidCredentialLength(const String& credential) {
  return credential.length() > 0 && credential.length() < 64;
}

//----------------------------------------------------------------------------//
// Output Function λ: Q → Γ (generates follow-up inputs)
//----------------------------------------------------------------------------//

// Output function λ: Q → Γ - generates follow-up inputs based on state changes
// This is where I/O happens in response to state transitions
// Returns follow-up input if needed, or INPUT_NONE
Input outputFunction(const AppState& oldState, const AppState& newState) {
  // Update LED indicators when mode changes
  if (newState.mode != oldState.mode) {
    updateLEDs(newState);  // Hardware I/O: control LED pins
  }
  
  // Persist credentials to flash when they change
  if (newState.credentialsChanged && !oldState.credentialsChanged) {
    saveCredentials(&newState.credentials);  // Flash I/O: write to storage
  }
  
  // Initiate WiFi connection when shouldReconnect flag is set
  if (newState.shouldReconnect && !oldState.shouldReconnect) {
    Serial.println("Initiating WiFi connection...");  // Serial I/O
    connectWiFi(&newState.credentials);               // Network I/O: start connection
    // Return action to clear the shouldReconnect flag
    return Input::connectionStarted();
  }
  
  // Update serial UI when mode changes
  if (newState.mode != oldState.mode) {
    renderUI(newState);  // Serial I/O: display status
  }
  
  return Input::none();  // No follow-up input needed
}

// Update LED indicators based on current application mode
void updateLEDs(const AppState& state) {
  switch (state.mode) {
    case MODE_CONNECTED:
      // Solid on when connected
      digitalWrite(wifi_led_pin, HIGH);
      break;
    case MODE_CONNECTING:
      // Blink at 2Hz during connection attempt
      digitalWrite(wifi_led_pin, (millis() / 250) % 2);  // Toggle every 250ms
      break;
    default:
      // Off for all other modes (disconnected, initializing, entering credentials)
      digitalWrite(wifi_led_pin, LOW);
      break;
  }
}

// Display appropriate UI messages based on current mode
void renderUI(const AppState& state) {
  switch (state.mode) {
    case MODE_CONNECTED:
      // Show network details and available commands
      printCurrentNet();  // Display SSID, IP, signal strength, etc.
      Serial.println("Send 'c' to change credentials.");
      break;
    case MODE_DISCONNECTED:
      // Show retry and credential change options
      Serial.println("Not connected. Send 'r' to retry or 'c' to change credentials.");
      break;
    case MODE_CONNECTING:
      // Simple status message during connection attempt
      Serial.println("Connecting...");
      break;
    case MODE_ENTERING_CREDENTIALS:
      // No message here - credential entry function handles its own prompts
      break;
    case MODE_INITIALIZING:
      // Startup message
      Serial.println("Initializing...");
      break;
  }
}

//----------------------------------------------------------------------------//
// Input Symbol Generation
//----------------------------------------------------------------------------//

// Read events from environment and convert to Input symbols
// This is the input layer of the Moore machine
Input readEvents() {
  const AppState& state = g_machine.getState();
  
  // Check for user input via serial (highest priority)
  char input = readSingleChar();
  if (input != '\0') {
    return parseUserInput(input, state.mode);  // Convert char to Input
  }
  
  // Check if tick timer has expired
  if (g_tickTimer.expired()) {
    g_tickTimer.restart();
    return Input::tick();
  }
  
  // Check for reset button press (optional)
  if (g_resetButton.wasPressed()) {
    return Input::requestCredentials();
  }
  
  return Input::none();
}

//----------------------------------------------------------------------------//
// State Observers (Reactive UI Updates)
//----------------------------------------------------------------------------//

// Observer: React to successful WiFi connection
void observeConnectedState(const AppState& oldState, const AppState& newState) {
  // Only trigger when transitioning TO connected state
  if (oldState.mode != MODE_CONNECTED && newState.mode == MODE_CONNECTED) {
    Serial.println("✓ Successfully connected to WiFi!");
    Serial.print("IP address: ");    // Show assigned IP address
    Serial.println(WiFi.localIP());
  }
}

// Observer: React to WiFi disconnection
void observeDisconnectedState(const AppState& oldState, const AppState& newState) {
  // Only trigger when transitioning FROM connected TO disconnected
  if (oldState.mode == MODE_CONNECTED && newState.mode == MODE_DISCONNECTED) {
    Serial.println("✗ WiFi connection lost");
  }
}

// Observer: React to credential changes
void observeCredentialChanges(const AppState& oldState, const AppState& newState) {
  // Trigger when credentialsChanged flag is set (before persistence)
  if (!oldState.credentialsChanged && newState.credentialsChanged) {
    Serial.println("💾 Credentials saved successfully");
  }
}

// Note: State transitions are now handled by the MooreMachine automatically

//----------------------------------------------------------------------------//
// Main Event Loop
//----------------------------------------------------------------------------//

// Arduino main loop - called continuously after setup()
// This is the event loop that drives the entire Moore machine
void loop() {
  const AppState& state = g_machine.getState();
  
  // Debug logging of current state (only when DEBUG_ENABLED=1)
  DEBUG_PRINT("DEBUG: Loop iteration, mode=");
  DEBUG_PRINT(state.mode);
  DEBUG_PRINT(" (");
  DEBUG_PRINT(getModeString(state.mode));
  DEBUG_PRINT("), shouldReconnect=");
  DEBUG_PRINTLN(state.shouldReconnect);
  
  // Read events from environment (user input, hardware status)
  Input input = readEvents();
  
  if (input.type != INPUT_NONE) {
    DEBUG_PRINT("DEBUG: Input type=");
    DEBUG_PRINTLN(input.type);
    
    // Special handling for credential entry (only blocking operation)
    if (input.type == INPUT_REQUEST_CREDENTIALS) {
      g_machine.step(input);  // First, change to credential entry mode
      
      // Blocking credential prompt (breaks Redux pattern but necessary for UX)
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

// Display detailed information about current WiFi connection
void printCurrentNet() {
  // Display network name
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // Display router's MAC address (BSSID = Basic Service Set Identifier)
  byte bssid[6];
  WiFi.BSSID(bssid);  // Get 6-byte MAC address
  Serial.print("BSSID: ");
  printMacAddress(bssid);  // Format and print MAC address

  // Display signal strength in dBm (decibels relative to milliwatt)
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  // Display security protocol (WEP, WPA, WPA2, etc.)
  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);  // Print as hexadecimal
  
  // Show available user commands
  Serial.println("Send 'c' to change credentials.");
  Serial.println();  // Blank line for readability
}

// Format and print a 6-byte MAC address in standard notation
// Example output: "AA:BB:CC:DD:EE:FF"
void printMacAddress(byte mac[]) {
  // Loop through 6 bytes in reverse order (network byte order)
  for (int i = 5; i >= 0; i--) {
    // Add leading zero for single-digit hex values
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);  // Print byte as hexadecimal
    // Add colon separator except after last byte
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();  // End with newline
}
