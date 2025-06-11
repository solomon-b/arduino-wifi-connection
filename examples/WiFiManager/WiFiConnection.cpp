#include "WiFiConnection.h"
#include "WiFiCredentials.h"
#include "WiFiUI.h"
#include <WiFi.h>
#include <MooreArduino.h>

using namespace MooreArduino;

//----------------------------------------------------------------------------//
// External References
//----------------------------------------------------------------------------//

// Global utilities  
extern Timer g_tickTimer;       // Defined in main file  
extern Button g_resetButton;    // Defined in main file
extern MooreMachine<AppState, Input, Output> g_machine;  // Defined in main file

//----------------------------------------------------------------------------//
// WiFi Connection Functions
//----------------------------------------------------------------------------//

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
  
  // Don't block here - let the Moore machine tick system handle status polling
}

//----------------------------------------------------------------------------//
// Input Processing Functions
//----------------------------------------------------------------------------//

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

Input readEvents() {
  const AppState& state = g_machine.getState();
  
  // Check for user input via serial (highest priority)
  char input = readSingleChar();
  if (input != '\0') {
    return parseUserInput(input, state.mode);  // Convert char to Input
  }
  
  // Check for WiFi status changes (hardware polling happens here, not in transition function)
  int currentWifiStatus = WiFi.status();
  if (currentWifiStatus != state.wifiStatus) {
    Serial.print("DEBUG: WiFi status changed from ");
    Serial.print(state.wifiStatus);
    Serial.print(" to ");
    Serial.println(currentWifiStatus);
    return Input::wifiStatusChanged(currentWifiStatus);
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
