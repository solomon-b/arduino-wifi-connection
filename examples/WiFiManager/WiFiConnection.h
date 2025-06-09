#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

#include "WiFiTypes.h"

//----------------------------------------------------------------------------//
// WiFi Connection Management
//----------------------------------------------------------------------------//

/**
 * Initiate WiFi connection to specified network
 * Performs network scan first to verify target exists
 * @param creds Pointer to credentials for target network
 */
void connectWiFi(const Credentials* creds);

/**
 * Parse single character user input into Input symbols
 * Only accepts input that's valid for current mode
 * @param input Character received from serial
 * @param currentMode Current application mode
 * @return Input symbol for the character, or INPUT_NONE if invalid
 */
Input parseUserInput(char input, AppMode currentMode);

/**
 * Read events from environment and convert to Input symbols
 * This is the input layer of the Moore machine
 * @return Input symbol representing current environmental state
 */
Input readEvents();

#endif // WIFI_CONNECTION_H