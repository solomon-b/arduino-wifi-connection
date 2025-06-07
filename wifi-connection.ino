#include <WiFi.h>
#include "kvstore_global_api.h"
#include <mbed_error.h>

//----------------------------------------------------------------------------//

const char* KEY_SSID = "wifi_ssid";
const char* KEY_PASS = "wifi_pass";

char ssid[64] = {0};
char pass[64] = {0};
int status = WL_IDLE_STATUS;

const int power_led_pin = 2;
const int wifi_led_pin = 3;

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

  if (!loadCredentials()) {
    Serial.println("No stored credentials.");
    promptForCredentials();
  }

  connectWiFi();
}

void saveCredentials(const char* s, const char* p) {
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

bool loadCredentials() {
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

  memset(ssid, 0, sizeof(ssid));
  int read_ssid_result = kv_get(KEY_SSID, ssid, ssid_buffer.size, nullptr);

  if (read_ssid_result != MBED_SUCCESS) {
    Serial.print("'kv_get(KEY_SSID, ssid, sizeof(ssid), nullptr);' failed with error code ");
    Serial.println(read_ssid_result);
    while (true) {}
  }
  memset(pass, 0, sizeof(pass));
  int read_pass_result = kv_get(KEY_PASS, pass, pass_buffer.size, nullptr);

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

void promptForCredentials() {
  flushSerialInput();

  // Read SSID
  Serial.println("Enter SSID:");
  // Wait for user input
  while (!Serial.available());
  String ssid_str = Serial.readStringUntil('\n');
  // Remove leading/trailing whitespace
  ssid_str.trim();

  // Validate length
  if (ssid_str.length() == 0 || ssid_str.length() >= 64) {
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

  if (pass_str.length() == 0 || pass_str.length() >= 64) {
    Serial.println("Invalid password length. Not saving.");
    return;
  }

  char pass_buf[64] = {0};
  pass_str.toCharArray(pass_buf, sizeof(pass_buf));

  // Update global buffers
  strncpy(ssid, ssid_buf, sizeof(ssid));
  strncpy(pass, pass_buf, sizeof(pass));

  saveCredentials(ssid_buf, pass_buf);
}

void connectWiFi() {
  Serial.print("Connecting to SSID: '");
  Serial.print(ssid);
  Serial.print("' (length: ");
  Serial.print(strlen(ssid));
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
    
    if (strcmp(WiFi.SSID(i), ssid) == 0) {
      networkFound = true;
      Serial.println("  ^ Target network found!");
    }
  }
  
  if (!networkFound) {
    Serial.println("ERROR: Target network not found in scan!");
    digitalWrite(wifi_led_pin, LOW);
    return;
  }

  status = WiFi.begin(ssid, pass);

  // Wait for connection with timeout
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(wifi_led_pin, HIGH);
    Serial.println("\nConnected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    digitalWrite(wifi_led_pin, LOW);
    Serial.println("\nFailed to connect.");
    Serial.print("WiFi status: ");
    Serial.println(WiFi.status());
  }
}

//----------------------------------------------------------------------------//

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    printCurrentNet();
    if (Serial.available()) {
      char input = Serial.read();
      if (input == 'c' || input == 'C') {
        Serial.println("Entering new credentials...");
        promptForCredentials();
        connectWiFi();
      }
      // Clear any remaining input
      while (Serial.available()) Serial.read();
    }
  } else {
    Serial.println("Not connected. Send 'r' to retry or 'c' to change credentials.");
    if (Serial.available()) {
      char input = Serial.read();
      if (input == 'r' || input == 'R') {
        Serial.println("Retrying connection...");
        connectWiFi();
      } else if (input == 'c' || input == 'C') {
        Serial.println("Entering new credentials...");
        promptForCredentials();
        connectWiFi();
      }
      // Clear any remaining input
      while (Serial.available()) Serial.read();
    }
  }
  delay(500);
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
