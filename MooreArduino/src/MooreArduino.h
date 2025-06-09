#ifndef MOORE_ARDUINO_H
#define MOORE_ARDUINO_H

/**
 * MooreArduino - Moore Machine implementation for Arduino
 * 
 * This library implements Moore finite state machines for embedded systems.
 * A Moore machine is a finite automaton where outputs depend only on the current state.
 * 
 * A Moore machine M = (Q, Σ, δ, λ, q₀) where:
 * - Q is a finite set of states
 * - Σ is a finite set of input symbols
 * - δ: Q × Σ → Q is the state transition function  
 * - λ: Q → Γ is the output function
 * - q₀ is the initial state
 * 
 * Key Components:
 * - MooreMachine: Core finite state machine implementation
 * - Timer: Non-blocking timer utilities
 * - Button: Debounced button input handling  
 * - AsyncOp: Async operation tracking with timeouts
 * 
 * Usage:
 *   #include <MooreArduino.h>
 *   using namespace MooreArduino;
 *   
 *   // Define your state space Q and input alphabet Σ
 *   struct AppState { int mode; };    // Q
 *   struct Input { int type; };       // Σ
 *   
 *   // Define transition function δ: Q × Σ → Q
 *   AppState transition(const AppState& state, const Input& input) { ... }
 *   
 *   // Create Moore machine and utilities
 *   MooreMachine<AppState, Input> machine(transition, AppState());
 *   Timer heartbeat(1000);
 *   Button powerButton(2);
 * 
 * Note: Redux is actually a specific implementation of Moore machines.
 * This library provides the underlying computational model directly.
 */

#include "MooreMachine.h"
#include "Timer.h"
#include "Button.h"
#include "AsyncOp.h"

// Version info
#define MOORE_ARDUINO_VERSION_MAJOR 1
#define MOORE_ARDUINO_VERSION_MINOR 0
#define MOORE_ARDUINO_VERSION_PATCH 0

#endif // MOORE_ARDUINO_H