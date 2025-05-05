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

  Serial.begin(115200);
  while (!Serial);

  delay(1000);

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
  Serial.print("Connecting to ");
  Serial.println(ssid);

  status = WiFi.begin(ssid, pass);

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(wifi_led_pin, HIGH);
    Serial.println("\nConnected!");
  } else {
    digitalWrite(wifi_led_pin, LOW);
    Serial.println("\nFailed to connect.");
  }
}

//----------------------------------------------------------------------------//

void loop() {
  printCurrentNet();
  delay(500 );
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
