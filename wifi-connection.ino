#include <WiFi.h>
#include "kvstore_global_api.h"
#include <mbed_error.h>

//----------------------------------------------------------------------------//
// Constants
//----------------------------------------------------------------------------//

const char* KEY_SSID = "wifi_ssid";
const char* KEY_PASS = "wifi_pass";

const int power_led_pin = 2;
const int wifi_led_pin = 3;

//----------------------------------------------------------------------------//
// Types
//----------------------------------------------------------------------------//

enum ActionType {
  ACTION_NONE,
  ACTION_RETRY_CONNECTION,
  ACTION_REQUEST_CREDENTIALS,
  ACTION_CREDENTIALS_ENTERED,
  ACTION_CONNECTION_STARTED,
  ACTION_WIFI_CONNECTED,
  ACTION_WIFI_DISCONNECTED,
  ACTION_TICK
};

struct Credentials {
  char ssid[64];
  char pass[64];
  
  bool isEmpty() const {
    return ssid[0] == '\0';
  }
  
  bool isValid() const {
    return strlen(ssid) > 0 && strlen(ssid) < 64 && 
           strlen(pass) > 0 && strlen(pass) < 64;
  }
};

enum AppMode {
  MODE_INITIALIZING,
  MODE_CONNECTING,
  MODE_CONNECTED,
  MODE_DISCONNECTED,
  MODE_ENTERING_CREDENTIALS
};

struct AppState {
  Credentials credentials;
  AppMode mode;
  int wifiStatus;
  unsigned long lastUpdate;
  bool credentialsChanged;
  bool shouldReconnect;
  
  AppState() : mode(MODE_INITIALIZING), wifiStatus(WL_IDLE_STATUS), 
               lastUpdate(0), credentialsChanged(false), shouldReconnect(false) {
    credentials.ssid[0] = '\0';
    credentials.pass[0] = '\0';
  }
};

struct Action {
  ActionType type;
  Credentials newCredentials;
  int wifiStatus;
  
  static Action none() {
    Action a;
    a.type = ACTION_NONE;
    return a;
  }
  
  static Action retryConnection() {
    Action a;
    a.type = ACTION_RETRY_CONNECTION;
    return a;
  }
  
  static Action requestCredentials() {
    Action a;
    a.type = ACTION_REQUEST_CREDENTIALS;
    return a;
  }
  
  static Action credentialsEntered(const Credentials& creds) {
    Action a;
    a.type = ACTION_CREDENTIALS_ENTERED;
    a.newCredentials = creds;
    return a;
  }
  
  static Action connectionStarted() {
    Action a;
    a.type = ACTION_CONNECTION_STARTED;
    return a;
  }
  
  static Action wifiStatusChanged(int status) {
    Action a;
    a.type = (status == WL_CONNECTED) ? ACTION_WIFI_CONNECTED : ACTION_WIFI_DISCONNECTED;
    a.wifiStatus = status;
    return a;
  }
  
  static Action tick() {
    Action a;
    a.type = ACTION_TICK;
    return a;
  }
};

//----------------------------------------------------------------------------//
// State
//----------------------------------------------------------------------------//

AppState g_currentState;

//----------------------------------------------------------------------------//

void setup() {
  pinMode(wifi_led_pin, OUTPUT);
  pinMode(power_led_pin, OUTPUT);
  digitalWrite(power_led_pin, HIGH);
  digitalWrite(wifi_led_pin, LOW);

  Serial.begin(9600);
  while (!Serial);

  delay(1000);

  Serial.println("Checking WiFi module...");
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("ERROR: WiFi module not detected!");
    while (true) {
      digitalWrite(wifi_led_pin, HIGH);
      delay(100);
      digitalWrite(wifi_led_pin, LOW);
      delay(100);
    }
  }
  
  Serial.print("WiFi module status: ");
  Serial.println(WiFi.status());
  
  // Get firmware version
  Serial.print("WiFi firmware: ");
  Serial.println(WiFi.firmwareVersion());

  if (!loadCredentials(&g_currentState.credentials)) {
    Serial.println("No stored credentials.");
    dispatch(Action::requestCredentials());
  } else {
    dispatch(Action::retryConnection());
  }
}

void saveCredentials(const Credentials* creds) {
  const char* s = creds->ssid;
  const char* p = creds->pass;
  
  size_t ssid_size = strlen(s) + 1;
  int set_ssid_result = kv_set(KEY_SSID, s, ssid_size, 0);

  if (set_ssid_result != MBED_SUCCESS) {
    Serial.print("'kv_set(KEY_SSID, s, ssid_size, 0)' failed with error code ");
    Serial.print(set_ssid_result);
    while (true) {}
  }

  size_t pass_size = strlen(p) + 1;
  int set_pass_result = kv_set(KEY_PASS, p, pass_size, 0);

  if (set_pass_result != MBED_SUCCESS) {
    Serial.print("'kv_set(KEY_PASS, p, pass_size, 0)' failed with error code ");
    Serial.print(set_pass_result);
    while (true) {}
  }
}

bool loadCredentials(Credentials* creds) {
  kv_info_t ssid_buffer;
  kv_info_t pass_buffer;

  int get_ssid_result = kv_get_info(KEY_SSID, &ssid_buffer);
  int get_pass_result = kv_get_info(KEY_PASS, &pass_buffer);

  if (get_ssid_result == MBED_ERROR_ITEM_NOT_FOUND || get_pass_result == MBED_ERROR_ITEM_NOT_FOUND) {
    return false;
  } else if (get_ssid_result != MBED_SUCCESS ) {
    Serial.print("kv_get_info failed for KEY_SSID with ");
    Serial.println(get_ssid_result);
    while (true) {}
  } else if (get_pass_result != MBED_SUCCESS) {
    Serial.print("kv_get_info failed for KEY_PASS with ");
    Serial.println(get_pass_result);
    while (true) {}
  }

  memset(creds->ssid, 0, sizeof(creds->ssid));
  int read_ssid_result = kv_get(KEY_SSID, creds->ssid, ssid_buffer.size, nullptr);

  if (read_ssid_result != MBED_SUCCESS) {
    Serial.print("'kv_get(KEY_SSID, ssid, sizeof(ssid), nullptr);' failed with error code ");
    Serial.println(read_ssid_result);
    while (true) {}
  }
  memset(creds->pass, 0, sizeof(creds->pass));
  int read_pass_result = kv_get(KEY_PASS, creds->pass, pass_buffer.size, nullptr);

  if (read_pass_result != MBED_SUCCESS) {
    Serial.print("'kv_get(KEY_PASS, pass, sizeof(ssid), nullptr);' failed with error code ");
    Serial.println(read_pass_result);
    while (true) {}
  }

  return (get_ssid_result == MBED_SUCCESS && get_pass_result == MBED_SUCCESS);
}

void flushSerialInput() {
  while (Serial.available()) Serial.read();
}

bool promptForCredentialsBlocking(Credentials* creds) {
  flushSerialInput();

  // Read SSID
  Serial.println("Enter SSID:");
  while (!Serial.available());
  String ssid_str = Serial.readStringUntil('\n');
  ssid_str.trim();

  if (!isValidCredentialLength(ssid_str)) {
    Serial.println("Invalid SSID length. Aborting.");
    return false;
  }

  ssid_str.toCharArray(creds->ssid, sizeof(creds->ssid));
  flushSerialInput();

  // Read Password  
  Serial.println("Enter Password:");
  while (!Serial.available());
  String pass_str = Serial.readStringUntil('\n');
  pass_str.trim();

  if (!isValidCredentialLength(pass_str)) {
    Serial.println("Invalid password length. Aborting.");
    return false;
  }

  pass_str.toCharArray(creds->pass, sizeof(creds->pass));
  return true;
}

void promptForCredentials(Credentials* creds) {
  flushSerialInput();

  // Read SSID
  Serial.println("Enter SSID:");
  // Wait for user input
  while (!Serial.available());
  String ssid_str = Serial.readStringUntil('\n');
  // Remove leading/trailing whitespace
  ssid_str.trim();

  // Validate length
  if (!isValidCredentialLength(ssid_str)) {
    Serial.println("Invalid SSID length. Not saving.");
    return;
  }

  // Copy to local C-string buffer
  char ssid_buf[64] = {0};
  ssid_str.toCharArray(ssid_buf, sizeof(ssid_buf));

  flushSerialInput();

  // Read Password
  Serial.println("Enter Password:");
  while (!Serial.available());
  String pass_str = Serial.readStringUntil('\n');
  pass_str.trim();

  if (!isValidCredentialLength(pass_str)) {
    Serial.println("Invalid password length. Not saving.");
    return;
  }

  char pass_buf[64] = {0};
  pass_str.toCharArray(pass_buf, sizeof(pass_buf));

  // Update credentials struct
  strncpy(creds->ssid, ssid_buf, sizeof(creds->ssid));
  strncpy(creds->pass, pass_buf, sizeof(creds->pass));

  saveCredentials(creds);
}

void connectWiFi(const Credentials* creds) {
  Serial.print("Connecting to SSID: '");
  Serial.print(creds->ssid);
  Serial.print("' (length: ");
  Serial.print(strlen(creds->ssid));
  Serial.println(")");
  
  // Scan for networks first
  Serial.println("Scanning for networks...");
  Serial.println("This may take 10-15 seconds...");
  int numNetworks = WiFi.scanNetworks();
  Serial.print("Scan completed. Found ");
  Serial.print(numNetworks);
  Serial.println(" networks:");
  
  if (numNetworks == 0) {
    Serial.println("No networks found. Possible issues:");
    Serial.println("1. WiFi antenna not connected");
    Serial.println("2. WiFi module hardware problem"); 
    Serial.println("3. Distance from access point too far");
    Serial.println("4. WiFi module not properly initialized");
  }
  
  bool networkFound = false;
  for (int i = 0; i < numNetworks; i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" (");
    Serial.print(WiFi.RSSI(i));
    Serial.println(" dBm)");
    
    if (strcmp(WiFi.SSID(i), creds->ssid) == 0) {
      networkFound = true;
      Serial.println("  ^ Target network found!");
    }
  }
  
  if (!networkFound) {
    Serial.println("ERROR: Target network not found in scan!");
    digitalWrite(wifi_led_pin, LOW);
    return;
  }

  Serial.println("Starting WiFi connection...");
  WiFi.begin(creds->ssid, creds->pass);
  
  // Don't block - let the tick system handle status updates
}

//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// Pure Functions
//----------------------------------------------------------------------------//

Action parseUserInput(char input, AppMode currentMode) {
  switch (input) {
    case 'r':
    case 'R':
      return (currentMode == MODE_DISCONNECTED) ? Action::retryConnection() : Action::none();
    case 'c':
    case 'C':
      return Action::requestCredentials();
    default:
      return Action::none();
  }
}

//----------------------------------------------------------------------------//
// Pure Reducers (State Transitions)
//----------------------------------------------------------------------------//

AppState reduce(const AppState& state, const Action& action) {
  AppState newState = state;
  newState.lastUpdate = millis();
  
  switch (action.type) {
    case ACTION_NONE:
      return newState;
      
    case ACTION_REQUEST_CREDENTIALS:
      newState.mode = MODE_ENTERING_CREDENTIALS;
      return newState;
      
    case ACTION_CREDENTIALS_ENTERED:
      newState.credentials = action.newCredentials;
      newState.credentialsChanged = true;
      newState.shouldReconnect = true;
      newState.mode = MODE_CONNECTING;
      return newState;
      
    case ACTION_CONNECTION_STARTED:
      newState.shouldReconnect = false;
      return newState;
      
    case ACTION_RETRY_CONNECTION:
      newState.shouldReconnect = true;
      newState.mode = MODE_CONNECTING;
      return newState;
      
    case ACTION_WIFI_CONNECTED:
      newState.mode = MODE_CONNECTED;
      newState.wifiStatus = action.wifiStatus;
      newState.shouldReconnect = false;
      return newState;
      
    case ACTION_WIFI_DISCONNECTED:
      newState.mode = MODE_DISCONNECTED;
      newState.wifiStatus = action.wifiStatus;
      return newState;
      
    case ACTION_TICK: {
      // Check if WiFi status changed
      int currentWifiStatus = WiFi.status();
      if (currentWifiStatus != newState.wifiStatus) {
        newState.wifiStatus = currentWifiStatus;
        if (currentWifiStatus == WL_CONNECTED && newState.mode != MODE_CONNECTED) {
          newState.mode = MODE_CONNECTED;
        } else if (currentWifiStatus != WL_CONNECTED && newState.mode == MODE_CONNECTED) {
          newState.mode = MODE_DISCONNECTED;
        }
      }
      return newState;
    }
      
    default:
      return newState;
  }
}

//----------------------------------------------------------------------------//
// Pure Helper Functions
//----------------------------------------------------------------------------//

char readSingleChar() {
  if (!Serial.available()) return '\0';
  char input = Serial.read();
  flushSerialInput();
  return input;
}

bool isValidCredentialLength(const String& credential) {
  return credential.length() > 0 && credential.length() < 64;
}

//----------------------------------------------------------------------------//
// Side Effects System
//----------------------------------------------------------------------------//

Action performSideEffects(const AppState& oldState, const AppState& newState) {
  // Handle LED updates
  if (newState.mode != oldState.mode) {
    updateLEDs(newState);
  }
  
  // Handle credential persistence
  if (newState.credentialsChanged && !oldState.credentialsChanged) {
    saveCredentials(&newState.credentials);
  }
  
  // Handle WiFi connection
  if (newState.shouldReconnect && !oldState.shouldReconnect) {
    Serial.println("Initiating WiFi connection...");
    connectWiFi(&newState.credentials);
    // Return action to reset shouldReconnect
    return Action::connectionStarted();
  }
  
  // Handle UI updates
  if (newState.mode != oldState.mode) {
    renderUI(newState);
  }
  
  return Action::none();
}

void updateLEDs(const AppState& state) {
  switch (state.mode) {
    case MODE_CONNECTED:
      digitalWrite(wifi_led_pin, HIGH);
      break;
    case MODE_CONNECTING:
      // Blink during connection
      digitalWrite(wifi_led_pin, (millis() / 250) % 2);
      break;
    default:
      digitalWrite(wifi_led_pin, LOW);
      break;
  }
}

void renderUI(const AppState& state) {
  switch (state.mode) {
    case MODE_CONNECTED:
      printCurrentNet();
      Serial.println("Send 'c' to change credentials.");
      break;
    case MODE_DISCONNECTED:
      Serial.println("Not connected. Send 'r' to retry or 'c' to change credentials.");
      break;
    case MODE_CONNECTING:
      Serial.println("Connecting...");
      break;
    case MODE_ENTERING_CREDENTIALS:
      // This will be handled by promptForCredentials
      break;
    case MODE_INITIALIZING:
      Serial.println("Initializing...");
      break;
  }
}

//----------------------------------------------------------------------------//
// Event Dispatch System
//----------------------------------------------------------------------------//

Action readEvents(const AppState& state) {
  // Check for user input
  char input = readSingleChar();
  if (input != '\0') {
    return parseUserInput(input, state.mode);
  }
  
  // Always return a tick action to update state
  return Action::tick();
}

//----------------------------------------------------------------------------//
// State Observers (Reactive UI Updates)
//----------------------------------------------------------------------------//

typedef void (*StateObserver)(const AppState& oldState, const AppState& newState);

void observeConnectedState(const AppState& oldState, const AppState& newState) {
  if (oldState.mode != MODE_CONNECTED && newState.mode == MODE_CONNECTED) {
    Serial.println("✓ Successfully connected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void observeDisconnectedState(const AppState& oldState, const AppState& newState) {
  if (oldState.mode == MODE_CONNECTED && newState.mode == MODE_DISCONNECTED) {
    Serial.println("✗ WiFi connection lost");
  }
}

void observeCredentialChanges(const AppState& oldState, const AppState& newState) {
  if (!oldState.credentialsChanged && newState.credentialsChanged) {
    Serial.println("💾 Credentials saved successfully");
  }
}

// Array of observers - easily extensible
StateObserver g_observers[] = {
  observeConnectedState,
  observeDisconnectedState,
  observeCredentialChanges
};

const int g_observerCount = sizeof(g_observers) / sizeof(g_observers[0]);

void notifyObservers(const AppState& oldState, const AppState& newState) {
  for (int i = 0; i < g_observerCount; i++) {
    g_observers[i](oldState, newState);
  }
}

//----------------------------------------------------------------------------//
// Central Dispatch Function
//----------------------------------------------------------------------------//

void dispatch(Action action) {
  AppState oldState = g_currentState;
  g_currentState = reduce(g_currentState, action);
  
  // Execute side effects and get any follow-up action
  Action followUpAction = performSideEffects(oldState, g_currentState);
  
  // Notify reactive observers
  notifyObservers(oldState, g_currentState);
  
  // Dispatch follow-up action if needed
  if (followUpAction.type != ACTION_NONE) {
    // Recursive dispatch for follow-up actions
    AppState prevState = g_currentState;
    g_currentState = reduce(g_currentState, followUpAction);
    notifyObservers(prevState, g_currentState);
  }
}

//----------------------------------------------------------------------------//
// Main Event Loop
//----------------------------------------------------------------------------//

void loop() {
  Serial.print("DEBUG: Loop iteration, mode=");
  Serial.print(g_currentState.mode);
  Serial.print(", shouldReconnect=");
  Serial.println(g_currentState.shouldReconnect);
  
  Action action = readEvents(g_currentState);
  Serial.print("DEBUG: Action type=");
  Serial.println(action.type);
  
  // Handle credential entry as a special case (blocking operation)
  if (action.type == ACTION_REQUEST_CREDENTIALS) {
    dispatch(action); // Change to credential entry mode
    
    Credentials newCreds;
    if (promptForCredentialsBlocking(&newCreds)) {
      Serial.println("DEBUG: About to dispatch credentialsEntered");
      dispatch(Action::credentialsEntered(newCreds));
      Serial.println("DEBUG: After dispatch credentialsEntered");
      // Continue processing - don't return, let the loop continue
    } else {
      // If credential entry failed, go back to previous state
      dispatch(Action::tick()); // This will update the mode based on current WiFi status
    }
  } else {
    dispatch(action);
  }
  
  delay(100); // Reduced delay for more responsive UI
}

void printCurrentNet() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the MAC address of the router you're attached to:
  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  // print the encryption type:
  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
  
  Serial.println("Send 'c' to change credentials.");
  Serial.println();
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}
