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
 * 
 * Usage:
 *   // Define your state space and input alphabet
 *   struct AppState { int mode; };  // Q
 *   struct Input { int type; };     // Σ
 *   
 *   // Define your transition function δ: Q × Σ → Q
 *   AppState transitionFunction(const AppState& state, const Input& input) {
 *     AppState newState = state;
 *     // transition logic here
 *     return newState;
 *   }
 *   
 *   // Create Moore machine
 *   MooreMachine<AppState, Input> machine(transitionFunction, AppState());
 *   
 *   // Process inputs
 *   machine.step(myInput);
 *   AppState currentState = machine.getState();
 */
template<typename State, typename Input>
class MooreMachine {
public:
  // Function pointer types for Moore machine components
  typedef State (*TransitionFunction)(const State&, const Input&);  // δ: Q × Σ → Q
  typedef Input (*OutputFunction)(const State&, const State&);      // λ: Q → Γ (generates follow-up inputs)
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
    
    // 1. Apply input to current state via transition function δ
    currentState = delta(currentState, input);
    
    // 2. Execute output function λ if defined (generates follow-up inputs)
    if (lambda) {
      Input followUpInput = lambda(oldState, currentState);
      
      // Process follow-up inputs (but prevent infinite loops)
      static int stepDepth = 0;
      if (stepDepth < 3) {  // Prevent deep recursion
        stepDepth++;
        step(followUpInput);  // Recursively process follow-up input
        stepDepth--;
      } else {
        // Prevent compiler warning by explicitly using the variable
        (void)followUpInput;
      }
    }
    
    // 3. Notify all state observers
    notifyObservers(oldState, currentState);
  }

  /**
   * Get current state q (read-only)
   */
  const State& getState() const {
    return currentState;
  }

  /**
   * Set output function λ (optional)
   * The output function can generate follow-up inputs based on state transitions
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