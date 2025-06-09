#ifndef REDUX_TIMER_H
#define REDUX_TIMER_H

#include <Arduino.h>

namespace MooreArduino {

/**
 * Simple non-blocking timer for Redux applications
 * 
 * Usage:
 *   Timer heartbeat(1000);  // 1 second timer
 *   heartbeat.start();
 *   
 *   if (heartbeat.expired()) {
 *     // Do something every second
 *     heartbeat.restart();
 *   }
 */
class Timer {
private:
  unsigned long interval;
  unsigned long lastTrigger;
  bool running;

public:
  /**
   * Create a timer with the specified interval in milliseconds
   */
  Timer(unsigned long intervalMs) 
    : interval(intervalMs), lastTrigger(0), running(false) {}

  /**
   * Start the timer from now
   */
  void start() {
    lastTrigger = millis();
    running = true;
  }

  /**
   * Check if the timer has expired
   */
  bool expired() const {
    return running && (millis() - lastTrigger >= interval);
  }

  /**
   * Stop the timer
   */
  void stop() {
    running = false;
  }

  /**
   * Restart the timer (same as start)
   */
  void restart() {
    start();
  }

  /**
   * Change the interval and restart
   */
  void setInterval(unsigned long newIntervalMs) {
    interval = newIntervalMs;
    start();
  }

  /**
   * Get the current interval
   */
  unsigned long getInterval() const {
    return interval;
  }

  /**
   * Check if timer is currently running
   */
  bool isRunning() const {
    return running;
  }

  /**
   * Get remaining time until expiration (0 if expired or stopped)
   */
  unsigned long remainingTime() const {
    if (!running) return 0;
    
    unsigned long elapsed = millis() - lastTrigger;
    if (elapsed >= interval) return 0;
    
    return interval - elapsed;
  }
};

} // namespace MooreArduino

#endif // REDUX_TIMER_H