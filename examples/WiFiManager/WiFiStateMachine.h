#ifndef WIFI_STATE_MACHINE_H
#define WIFI_STATE_MACHINE_H

#include "WiFiTypes.h"

//----------------------------------------------------------------------------//
// Moore Machine Core Functions
//----------------------------------------------------------------------------//

/**
 * Pure state transition function δ: Q × Σ → Q
 * This is the heart of the Moore machine - implements δ(q, σ) → q'
 * @param state Current state q
 * @param input Input symbol σ
 * @return New state q'
 */
AppState transitionFunction(const AppState& state, const Input& input);

/**
 * Pure output function λ: Q → Γ - generates effects based on current state
 * This implements the Moore machine property: outputs depend only on current state
 * @param state Current state q
 * @return Output to be executed
 */
Output outputFunction(const AppState& state);

/**
 * Execute effects produced by the Moore machine
 * This is where all I/O operations happen
 * @param effect Output to execute
 * @return Follow-up input if needed, or INPUT_NONE
 */
Input executeEffect(const Output& effect);

#endif // WIFI_STATE_MACHINE_H
