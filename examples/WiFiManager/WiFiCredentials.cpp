#include "WiFiCredentials.h"
#include "kvstore_global_api.h"
#include <mbed_error.h>

//----------------------------------------------------------------------------//
// Hardware Constants
//----------------------------------------------------------------------------//

// Key names for persistent storage in KVStore (flash memory)
const char* KEY_SSID = "wifi_ssid";  // Key for storing WiFi network name
const char* KEY_PASS = "wifi_pass";  // Key for storing WiFi password

//----------------------------------------------------------------------------//
// Credential Persistence Functions
//----------------------------------------------------------------------------//

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

//----------------------------------------------------------------------------//
// Serial Input Functions
//----------------------------------------------------------------------------//

void flushSerialInput() {
  while (Serial.available()) Serial.read();  // Read and discard all pending bytes
}

bool isValidCredentialLength(const String& credential) {
  return credential.length() > 0 && credential.length() < 64;
}

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