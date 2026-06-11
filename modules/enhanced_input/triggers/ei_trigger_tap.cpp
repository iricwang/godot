/**************************************************************************/
/*  ei_trigger_tap.cpp                                                    */
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

#include "ei_trigger_tap.h"

#include "core/object/class_db.h"

namespace ei {

void EITriggerTap::set_tap_release_time(double p_v) {
	_tap_release_time = p_v < 0.0 ? 0.0 : p_v;
}

double EITriggerTap::get_tap_release_time() const {
	return _tap_release_time;
}

EITrigger::UpdateResult EITriggerTap::update_state(
		TriggerRuntimeState &p_runtime,
		const EIValue &p_value,
		double p_delta_t,
		bool p_event_valid,
		bool p_pressed) const {
	(void)p_value;

	// --- Press event ---------------------------------------------------
	if (p_event_valid && p_pressed) {
		if (p_runtime.state == STATE_NONE) {
			// Rising edge. Record when the press started.
			p_runtime.state = STATE_ONGOING;
			p_runtime.hold_start_time = p_runtime.elapsed;
			return RESULT_STARTED;
		}
		// Re-press while we're already counting a tap (shouldn't happen
		// in a clean event stream, but handle it gracefully): restart.
		if (p_runtime.state == STATE_ONGOING) {
			p_runtime.hold_start_time = p_runtime.elapsed;
			return RESULT_NONE;
		}
		return RESULT_NONE;
	}

	// --- Release event -------------------------------------------------
	if (p_event_valid && !p_pressed) {
		if (p_runtime.state == STATE_ONGOING) {
			// Released within (or at) the tap window — it's a tap.
			p_runtime.state = STATE_NONE;
			p_runtime.hold_start_time = -1.0;
			return RESULT_TRIGGERED;
		}
		// Release while idle or already canceled: no event.
		return RESULT_NONE;
	}

	// --- Tick (event_valid=false) --------------------------------------
	p_runtime.elapsed += p_delta_t;

	if (p_runtime.state == STATE_ONGOING) {
		const double held_for = p_runtime.elapsed - p_runtime.hold_start_time;
		if (held_for >= _tap_release_time) {
			// Held too long — cancel. Use `>=` so that the spec
			// scenario "Tap(0.2) on Space held for 0.3s -> CANCELED at
			// t=0.2" fires the cancel at the boundary tick, not on a
			// release at the boundary (which we want to still count as
			// a successful tap — see release handler below).
			p_runtime.state = STATE_NONE;
			p_runtime.hold_start_time = -1.0;
			return RESULT_CANCELED;
		}
		// Still in the tap window. Report ONGOING so listeners can
		// distinguish "tap in progress" from "idle".
		return RESULT_ONGOING;
	}

	return RESULT_NONE;
}

String EITriggerTap::get_trigger_name() const {
	return "Tap";
}

void EITriggerTap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tap_release_time", "seconds"), &EITriggerTap::set_tap_release_time);
	ClassDB::bind_method(D_METHOD("get_tap_release_time"), &EITriggerTap::get_tap_release_time);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tap_release_time", PROPERTY_HINT_RANGE, "0.0,2.0,0.01"), "set_tap_release_time", "get_tap_release_time");
}

} // namespace ei
