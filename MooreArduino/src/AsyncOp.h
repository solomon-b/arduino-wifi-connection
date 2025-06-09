#ifndef REDUX_ASYNC_OP_H
#define REDUX_ASYNC_OP_H

#include <Arduino.h>

namespace MooreArduino {

/**
 * Track async operations with timeouts
 * 
 * Useful for operations like WiFi connections, HTTP requests, or any
 * long-running task that might fail or timeout.
 * 
 * Usage:
 *   AsyncOp wifiConnection;
 *   
 *   // Start 30-second WiFi connection
 *   wifiConnection.start(30000);
 *   WiFi.begin(ssid, password);
 *   
 *   // Check status
 *   if (wifiConnection.timedOut()) {
 *     // Handle timeout
 *     wifiConnection.finish();
 *   }
 */
class AsyncOp {
private:
  bool active;
  unsigned long startTime;
  unsigned long timeout;

public:
  /**
   * Create an inactive async operation
   */
  AsyncOp() : active(false), startTime(0), timeout(0) {}

  /**
   * Start the operation with a timeout in milliseconds
   */
  void start(unsigned long timeoutMs) {
    active = true;
    startTime = millis();
    timeout = timeoutMs;
  }

  /**
   * Mark the operation as finished (success or failure)
   */
  void finish() {
    active = false;
  }

  /**
   * Check if the operation has timed out
   */
  bool timedOut() const {
    return active && (millis() - startTime > timeout);
  }

  /**
   * Check if operation is currently active
   */
  bool isActive() const {
    return active;
  }

  /**
   * Get remaining time before timeout (0 if timed out or inactive)
   */
  unsigned long remainingTime() const {
    if (!active) return 0;
    
    unsigned long elapsed = millis() - startTime;
    if (elapsed >= timeout) return 0;
    
    return timeout - elapsed;
  }

  /**
   * Get elapsed time since start (0 if inactive)
   */
  unsigned long elapsedTime() const {
    if (!active) return 0;
    return millis() - startTime;
  }

  /**
   * Get the timeout duration
   */
  unsigned long getTimeout() const {
    return timeout;
  }

  /**
   * Check progress as percentage (0-100)
   * Returns 100 if timed out, 0 if inactive
   */
  int getProgress() const {
    if (!active) return 0;
    
    unsigned long elapsed = millis() - startTime;
    if (elapsed >= timeout) return 100;
    
    return (elapsed * 100) / timeout;
  }
};

} // namespace MooreArduino

#endif // REDUX_ASYNC_OP_H