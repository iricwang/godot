/**************************************************************************/
/*  ei_trigger.h                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/**************************************************************************/

#pragma once

// Sibling include (same `core/` directory) so this header resolves the same
// way when included from test_main.cpp (which lacks our module's CPPATH) and
// from the in-module .cpp files.
#include "ei_value.h"
#include "core/io/resource.h"

namespace ei {

// Abstract base for trigger state machines attached to an EIAction.
//
// Per spec §4.4 (enhanced-input.md v0.2):
// * Every trigger is a pure function over (runtime_state, value, dt,
//   event_valid, pressed) — no internal time source, no per-trigger
//   mutable state outside TriggerRuntimeState.
// * The trigger does NOT own its runtime state; EISubsystem stores
//   TriggerRuntimeState per (action, trigger) and passes it by reference
//   on every update. This lets the same trigger Resource be reused across
//   multiple actions without cross-talk.
// * update_state() is called on BOTH event arrival (p_event_valid=true)
//   AND on every _process tick (p_event_valid=false, p_delta_t>0). The
//   trigger decides what to do on each.
// * `p_delta_t` is wall-clock seconds (NOT frame count). It is
//   accumulated even on idle frames so Hold triggers fire correctly when
//   a key is held for 10 seconds with no other events.
//
// State machine (3 states) vs UpdateResult (6 values) distinction
// (see review.md B4):
//   * TriggerState is the internal FSM: where the trigger is right now.
//   * UpdateResult is the transition event for THIS call: what edge, if
//     any, did the trigger just cross.
//   A single update_state() returns ONE UpdateResult. EISubsystem
//   aggregates multiple triggers' results with priority
//   Triggered > Ongoing > Started > Completed > Canceled > None.
//
// Requirements (spec §4.4):
// * EITrigger subclasses SHALL be deterministic given
//   (runtime_state, value, dt, event_valid, pressed).
// * TriggerRuntimeState is mutated in place; it is NOT stored in the
//   trigger resource itself.
//
// This class is registered with ClassDB so that:
// 1. Subclasses can be saved as .tres resources alongside an EIAction.
// 2. The EditorResourcePicker in EIAction's inspector can filter on it.
// 3. TypedArray<EITrigger> serialization round-trips correctly.

class EITrigger : public Resource {
	GDCLASS(EITrigger, Resource);

public:
	// Internal FSM. Where is the trigger right now?
	enum TriggerState {
		STATE_NONE = 0, // idle; no active input
		STATE_ONGOING = 1, // input active, but threshold / window not yet met
		STATE_TRIGGERED = 2, // threshold / window met; the action is "active"
	};

	// Per-call transition event. What edge did this update_state() cross?
	// Spec §4.4 has 6 values; RESULT_NONE means "no event this call".
	enum UpdateResult {
		RESULT_NONE = 0,
		RESULT_STARTED = 1,
		RESULT_TRIGGERED = 2,
		RESULT_ONGOING = 3,
		RESULT_COMPLETED = 4,
		RESULT_CANCELED = 5,
	};

	// Per-(action, trigger) state owned by EISubsystem and passed by
	// reference to update_state(). The fields mirror spec §4.4 verbatim
	// except for two additions needed by the concrete triggers we ship in
	// P4 — see comments on each field.
	struct TriggerRuntimeState {
		// Current FSM state. All P4 triggers use this directly.
		TriggerState state = STATE_NONE;

		// For Hold: timestamp of the rising edge (press event). Relative
		// to the lifetime of this state instance — the trigger does not
		// have a wall clock; it accumulates p_delta_t into elapsed.
		// -1.0 means "not set" (initial).
		double hold_start_time = -1.0;

		// Generic "last time we fired" marker. Held at -1.0 until first
		// fire. The spec mentions this for Pulse-style triggers; P4
		// triggers don't use it directly but the field is kept for
		// forward compat.
		double last_fire_time = -1.0;

		// For DoubleTap: how many consecutive presses we've seen.
		int consecutive_tap_count = 0;

		// For DoubleTap: timestamp of the most recent tap (elapsed-based).
		// -1.0 means "no tap yet".
		double last_tap_time = -1.0;

		// P4 addition: cumulative p_delta_t since this state instance was
		// last reset. Updated by EITrigger base on every update_state()
		// call so concrete triggers have a relative clock. This is the
		// "wall clock" the spec §4.4 implicitly assumes — see review
		// notes B3 and B4. Concrete triggers compare (elapsed - x) against
		// their thresholds.
		//
		// Reset to 0.0 by EISubsystem whenever the trigger enters
		// STATE_NONE (so a fresh press starts at elapsed=0).
		double elapsed = 0.0;

		// P4 addition: for EITriggerRelease. Tracks whether the previous
		// value was non-zero, so we can detect the non-zero -> zero
		// transition. Reset to false when state goes to NONE.
		bool was_active = false;
	};

	// Pure function. Returns the transition event for this call.
	// Implementations are deterministic and MUST mutate p_runtime
	// in place; they MUST NOT touch `this`'s mutable state (the resource
	// is shared across actions).
	virtual UpdateResult update_state(
			TriggerRuntimeState &p_runtime,
			const EIValue &p_value,
			double p_delta_t,
			bool p_event_valid,
			bool p_pressed) const = 0;

	// Human-readable identifier for inspector display. Subclasses override.
	// (Marked redundant with Object::get_class() in review.md C3, but
	// kept in v1 to match spec §4.4 verbatim.)
	virtual String get_trigger_name() const = 0;

protected:
	static void _bind_methods();
};

} // namespace ei

VARIANT_ENUM_CAST(ei::EITrigger::TriggerState);
VARIANT_ENUM_CAST(ei::EITrigger::UpdateResult);
