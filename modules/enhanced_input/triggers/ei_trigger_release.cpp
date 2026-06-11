/**************************************************************************/
/*  ei_trigger_release.cpp                                                */
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

#include "ei_trigger_release.h"

#include "core/object/class_db.h"

namespace ei {

EITrigger::UpdateResult EITriggerRelease::update_state(
		TriggerRuntimeState &p_runtime,
		const EIValue &p_value,
		double p_delta_t,
		bool p_event_valid,
		bool p_pressed) const {
	(void)p_delta_t;
	(void)p_event_valid;
	(void)p_pressed;

	const bool is_zero_now = p_value.is_zero();

	if (is_zero_now && p_runtime.was_active) {
		// Falling edge: was non-zero, now zero. This is the moment to
		// fire. Transition the trigger state machine too: ONGOING ->
		// TRIGGERED, and report COMPLETED on the next call. (For a
		// Release trigger, "Completed" is effectively the same call —
		// the action goes from "held" to "released" in one frame. We
		// pick TRIGGERED for the fire event and let listeners on
		// COMPLETED see the next zero-valued call.)
		//
		// Spec wording: "fire Triggered on the frame the value
		// transitions from non-zero to zero". So we return TRIGGERED
		// (the canonical "action became active" event), not COMPLETED.
		// COMPLETED is what `Pressed` fires on release; for `Release`
		// the fire IS the release.
		p_runtime.state = STATE_TRIGGERED;
		p_runtime.was_active = false;
		return RESULT_TRIGGERED;
	}

	if (!is_zero_now) {
		// Value is non-zero: record it. We don't fire here — the spec
		// only fires on the falling edge.
		p_runtime.was_active = true;
		if (p_runtime.state == STATE_TRIGGERED) {
			// Edge case: a TRIGGERED Release trigger that sees a
			// non-zero value. Could happen if EISubsystem polls the
			// value multiple times per frame. Stay in TRIGGERED but
			// re-arm the edge detector (was_active is already true).
			return RESULT_ONGOING;
		}
		return RESULT_NONE;
	}

	// Zero and was_active=false: idle. No event.
	return RESULT_NONE;
}

String EITriggerRelease::get_trigger_name() const {
	return "Release";
}

void EITriggerRelease::_bind_methods() {
	// No config properties — Release has no parameters per spec.
}

} // namespace ei
