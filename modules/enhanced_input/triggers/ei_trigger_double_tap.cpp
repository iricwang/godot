/**************************************************************************/
/*  ei_trigger_double_tap.cpp                                             */
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

#include "ei_trigger_double_tap.h"

#include "core/object/class_db.h"

namespace ei {

void EITriggerDoubleTap::set_double_tap_time(double p_v) {
	_double_tap_time = p_v < 0.0 ? 0.0 : p_v;
}

double EITriggerDoubleTap::get_double_tap_time() const {
	return _double_tap_time;
}

EITrigger::UpdateResult EITriggerDoubleTap::update_state(
		TriggerRuntimeState &p_runtime,
		const EIValue &p_value,
		double p_delta_t,
		bool p_event_valid,
		bool p_pressed) const {
	(void)p_value;

	// --- Press event ---------------------------------------------------
	if (p_event_valid && p_pressed) {
		if (p_runtime.state == STATE_NONE) {
			// First press of a potential double-tap. Record it.
			p_runtime.state = STATE_ONGOING;
			p_runtime.consecutive_tap_count = 1;
			p_runtime.last_tap_time = p_runtime.elapsed;
			return RESULT_STARTED;
		}
		if (p_runtime.state == STATE_ONGOING) {
			// We have a pending first tap. Is this 2nd tap within the
			// window?
			const double since_last = p_runtime.elapsed - p_runtime.last_tap_time;
			if (since_last <= _double_tap_time) {
				// Double-tap! Fire TRIGGERED.
				p_runtime.state = STATE_TRIGGERED;
				p_runtime.consecutive_tap_count = 2;
				p_runtime.last_tap_time = p_runtime.elapsed;
				return RESULT_TRIGGERED;
			}
			// 2nd press came too late — treat it as a new 1st press.
			p_runtime.consecutive_tap_count = 1;
			p_runtime.last_tap_time = p_runtime.elapsed;
			return RESULT_STARTED;
		}
		if (p_runtime.state == STATE_TRIGGERED) {
			// 3rd press after a successful double-tap. If still within
			// the window, count it as a new 1st press; otherwise treat
			// it the same way (a 3rd press is a fresh candidate).
			p_runtime.consecutive_tap_count = 1;
			p_runtime.last_tap_time = p_runtime.elapsed;
			p_runtime.state = STATE_ONGOING;
			return RESULT_STARTED;
		}
		return RESULT_NONE;
	}

	// --- Release event -------------------------------------------------
	if (p_event_valid && !p_pressed) {
		if (p_runtime.state == STATE_TRIGGERED) {
			// Successful double-tap completed.
			p_runtime.state = STATE_NONE;
			p_runtime.consecutive_tap_count = 0;
			p_runtime.last_tap_time = -1.0;
			return RESULT_COMPLETED;
		}
		// Release while ONGOING: stay in ONGOING, still waiting for the
		// 2nd press. (UE behavior: don't reset on inter-tap release.)
		return RESULT_NONE;
	}

	// --- Tick (event_valid=false) --------------------------------------
	p_runtime.elapsed += p_delta_t;

	if (p_runtime.state == STATE_ONGOING) {
		const double since_last = p_runtime.elapsed - p_runtime.last_tap_time;
		if (since_last > _double_tap_time) {
			// No 2nd press came in time — cancel the pending tap.
			p_runtime.state = STATE_NONE;
			p_runtime.consecutive_tap_count = 0;
			p_runtime.last_tap_time = -1.0;
			return RESULT_CANCELED;
		}
		return RESULT_ONGOING;
	}

	if (p_runtime.state == STATE_TRIGGERED) {
		// Stay triggered until release.
		return RESULT_ONGOING;
	}

	return RESULT_NONE;
}

String EITriggerDoubleTap::get_trigger_name() const {
	return "DoubleTap";
}

void EITriggerDoubleTap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_double_tap_time", "seconds"), &EITriggerDoubleTap::set_double_tap_time);
	ClassDB::bind_method(D_METHOD("get_double_tap_time"), &EITriggerDoubleTap::get_double_tap_time);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "double_tap_time", PROPERTY_HINT_RANGE, "0.0,2.0,0.01"), "set_double_tap_time", "get_double_tap_time");
}

} // namespace ei
