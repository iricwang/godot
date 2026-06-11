/**************************************************************************/
/*  ei_trigger_hold.cpp                                                   */
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

#include "ei_trigger_hold.h"

#include "core/object/class_db.h"

namespace ei {

void EITriggerHold::set_hold_time_threshold(double p_v) {
	// Clamp to non-negative; zero would mean "fires on press like
	// Pressed", which is technically allowed but the editor should
	// steer users to EITriggerPressed for that case.
	_hold_time_threshold = p_v < 0.0 ? 0.0 : p_v;
}

double EITriggerHold::get_hold_time_threshold() const {
	return _hold_time_threshold;
}

EITrigger::UpdateResult EITriggerHold::update_state(
		TriggerRuntimeState &p_runtime,
		const EIValue &p_value,
		double p_delta_t,
		bool p_event_valid,
		bool p_pressed) const {
	(void)p_value;

	// --- Press event ---------------------------------------------------
	if (p_event_valid && p_pressed) {
		if (p_runtime.state == STATE_NONE) {
			// Rising edge. Record when the hold began (relative clock).
			p_runtime.state = STATE_ONGOING;
			p_runtime.hold_start_time = p_runtime.elapsed;
			return RESULT_STARTED;
		}
		// Repeat press event while holding: stay in current state, no
		// event. (We don't reset hold_start_time — the spec scenario
		// measures from the FIRST press.)
		return RESULT_NONE;
	}

	// --- Release event -------------------------------------------------
	if (p_event_valid && !p_pressed) {
		if (p_runtime.state == STATE_TRIGGERED) {
			// Normal completion: threshold was met before release.
			p_runtime.state = STATE_NONE;
			p_runtime.hold_start_time = -1.0;
			return RESULT_COMPLETED;
		}
		if (p_runtime.state == STATE_ONGOING) {
			// Released before threshold — user gave up.
			p_runtime.state = STATE_NONE;
			p_runtime.hold_start_time = -1.0;
			return RESULT_CANCELED;
		}
		// Release while idle.
		return RESULT_NONE;
	}

	// --- Tick (event_valid=false) --------------------------------------
	// Accumulate dt into the elapsed clock so we can measure hold time.
	p_runtime.elapsed += p_delta_t;

	if (p_runtime.state == STATE_ONGOING) {
		const double held_for = p_runtime.elapsed - p_runtime.hold_start_time;
		if (held_for >= _hold_time_threshold) {
			// Threshold met. Promote to TRIGGERED. The "first tick past
			// threshold" returns TRIGGERED; subsequent ticks (in the
			// branch below) return ONGOING.
			p_runtime.state = STATE_TRIGGERED;
			return RESULT_TRIGGERED;
		}
		// Still waiting for threshold; no event this tick.
		return RESULT_NONE;
	}

	if (p_runtime.state == STATE_TRIGGERED) {
		// Already triggered; report ONGOING each tick so listeners that
		// bind to ONGOING see the held state.
		return RESULT_ONGOING;
	}

	return RESULT_NONE;
}

String EITriggerHold::get_trigger_name() const {
	return "Hold";
}

void EITriggerHold::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_hold_time_threshold", "seconds"), &EITriggerHold::set_hold_time_threshold);
	ClassDB::bind_method(D_METHOD("get_hold_time_threshold"), &EITriggerHold::get_hold_time_threshold);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hold_time_threshold", PROPERTY_HINT_RANGE, "0.0,5.0,0.01"), "set_hold_time_threshold", "get_hold_time_threshold");
}

} // namespace ei
