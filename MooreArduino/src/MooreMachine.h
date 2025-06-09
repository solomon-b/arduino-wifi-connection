#ifndef MOORE_MACHINE_H
#define MOORE_MACHINE_H

#include <Arduino.h>

namespace MooreArduino {

/**
 * Moore Machine - Finite state machine where outputs depend only on current state
 * 
 * A Moore machine is defined as M = (Q, Σ, δ, λ, q₀) where:
 * - Q is a finite set of states
 * - Σ is a finite set of input symbols  
 * - δ: Q × Σ → Q is the state transition function
 * - λ: Q → Γ is the output function
 * - q₀ is the initial state
 * 
 * Template parameters:
 *   State - Your state space Q (the set of all possible states)
 *   Input - Your input alphabet Σ (the set of all possible inputs)
 *   Effect - Your output alphabet Γ (the set of all possible effects)
 * 
 * Usage:
 *   // Define your state space, input alphabet, and effect alphabet
 *   struct AppState { int mode; };     // Q
 *   struct Input { int type; };        // Σ
 *   struct Effect { int type; };       // Γ
 *   
 *   // Define your transition function δ: Q × Σ → Q
 *   AppState transitionFunction(const AppState& state, const Input& input) {
 *     AppState newState = state;
 *     // transition logic here
 *     return newState;
 *   }
 *   
 *   // Define your output function λ: Q → Γ
 *   Effect outputFunction(const AppState& state) {
 *     // generate effects based on current state
 *     return effect;
 *   }
 *   
 *   // Create Moore machine
 *   MooreMachine<AppState, Input, Effect> machine(transitionFunction, AppState());
 *   machine.setOutputFunction(outputFunction);
 *   
 *   // Main loop
 *   Effect effect = machine.getCurrentOutput();
 *   executeEffect(effect);  // Handle effects in main loop
 *   Input input = readEnvironment();
 *   machine.step(input);
 */
template<typename State, typename Input, typename Effect>
class MooreMachine {
public:
  // Function pointer types for Moore machine components
  typedef State (*TransitionFunction)(const State&, const Input&);  // δ: Q × Σ → Q
  typedef Effect (*OutputFunction)(const State&);                   // λ: Q → Γ
  typedef void (*StateObserver)(const State&, const State&);        // Observer pattern for state changes

private:
  State currentState;                    // Current state q ∈ Q
  TransitionFunction delta;              // State transition function δ
  OutputFunction lambda;                 // Output function λ (optional)
  
  // Observer management for reactive patterns
  static const int MAX_OBSERVERS = 8;
  StateObserver observers[MAX_OBSERVERS];
  int observerCount;

public:
  /**
   * Create a Moore machine with transition function and initial state
   * 
   * @param transitionFunc δ: Q × Σ → Q (state transition function)
   * @param initialState q₀ (initial state)
   */
  MooreMachine(TransitionFunction transitionFunc, const State& initialState)
    : currentState(initialState), delta(transitionFunc), lambda(nullptr), observerCount(0) {
    // Initialize observer array to null
    for (int i = 0; i < MAX_OBSERVERS; i++) {
      observers[i] = nullptr;
    }
  }

  /**
   * Process input through Moore machine - execute one step of computation
   * This implements the core Moore machine operation: δ(q, σ) → q'
   */
  void step(const Input& input) {
    if (!delta) return;
    
    State oldState = currentState;
    
    // Apply input to current state via transition function δ
    currentState = delta(currentState, input);
    
    // Notify all state observers
    notifyObservers(oldState, currentState);
  }

  /**
   * Get current state q (read-only)
   */
  const State& getState() const {
    return currentState;
  }

  /**
   * Get current output from output function λ: Q → Γ
   * Returns the effect that should be executed based on current state
   */
  Effect getCurrentOutput() const {
    if (lambda) {
      return lambda(currentState);
    }
    return Effect();  // Default-constructed effect if no output function
  }

  /**
   * Set output function λ: Q → Γ
   * The output function generates effects based on the current state
   */
  void setOutputFunction(OutputFunction outputFunc) {
    lambda = outputFunc;
  }

  /**
   * Add a state observer function
   * Observers are notified whenever the machine transitions to a new state
   * Returns true if added successfully, false if observer array is full
   */
  bool addStateObserver(StateObserver observer) {
    if (observerCount >= MAX_OBSERVERS || !observer) {
      return false;
    }
    
    observers[observerCount] = observer;
    observerCount++;
    return true;
  }

  /**
   * Remove a state observer function
   */
  bool removeStateObserver(StateObserver observer) {
    for (int i = 0; i < observerCount; i++) {
      if (observers[i] == observer) {
        // Shift remaining observers down
        for (int j = i; j < observerCount - 1; j++) {
          observers[j] = observers[j + 1];
        }
        observerCount--;
        observers[observerCount] = nullptr;
        return true;
      }
    }
    return false;
  }

  /**
   * Get number of registered state observers
   */
  int getObserverCount() const {
    return observerCount;
  }

private:
  /**
   * Notify all observers of state transition
   */
  void notifyObservers(const State& oldState, const State& newState) {
    for (int i = 0; i < observerCount; i++) {
      if (observers[i]) {
        observers[i](oldState, newState);
      }
    }
  }
};

} // namespace MooreArduino

#endif // MOORE_MACHINE_H