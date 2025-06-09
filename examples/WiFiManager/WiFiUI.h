#ifndef WIFI_UI_H
#define WIFI_UI_H

#include "WiFiTypes.h"

//----------------------------------------------------------------------------//
// User Interface and Display
//----------------------------------------------------------------------------//

/**
 * Update LED indicators based on current application mode
 * @param mode Current application mode
 */
void updateLEDs(AppMode mode);

/**
 * Display appropriate UI messages based on current mode
 * @param mode Current application mode
 */
void renderUI(AppMode mode);

/**
 * Display detailed information about current WiFi connection
 * Shows SSID, IP address, signal strength, encryption type
 */
void printCurrentNet();

/**
 * Format and print a 6-byte MAC address in standard notation
 * Example output: "AA:BB:CC:DD:EE:FF"
 * @param mac Array of 6 bytes representing MAC address
 */
void printMacAddress(byte mac[]);

/**
 * Read single character from serial input (pure function)
 * @return Character from serial, or '\0' if no input available
 */
char readSingleChar();

//----------------------------------------------------------------------------//
// State Observers (Reactive UI Updates)
//----------------------------------------------------------------------------//

/**
 * Observer: React to successful WiFi connection
 * @param oldState Previous state
 * @param newState Current state
 */
void observeConnectedState(const AppState& oldState, const AppState& newState);

/**
 * Observer: React to WiFi disconnection
 * @param oldState Previous state
 * @param newState Current state
 */
void observeDisconnectedState(const AppState& oldState, const AppState& newState);

/**
 * Observer: React to credential changes
 * @param oldState Previous state
 * @param newState Current state
 */
void observeCredentialChanges(const AppState& oldState, const AppState& newState);

//----------------------------------------------------------------------------//
// Debug Helper Functions
//----------------------------------------------------------------------------//

#if DEBUG_ENABLED
/**
 * Convert enum values to human-readable strings
 * Only compiled when DEBUG_ENABLED=1 to save memory in production
 * @param mode Application mode to convert
 * @return String representation of the mode
 */
const char* getModeString(AppMode mode);
#endif

#endif // WIFI_UI_H