/**************************************************************************/
/*  ei_trigger_pressed.cpp                                                */
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

#include "ei_trigger_pressed.h"

#include "core/object/class_db.h"

namespace ei {

EITrigger::UpdateResult EITriggerPressed::update_state(
		TriggerRuntimeState &p_runtime,
		const EIValue &p_value,
		double p_delta_t,
		bool p_event_valid,
		bool p_pressed) const {
	(void)p_value; // Pressed doesn't read the value; it's purely event-driven.

	// Accumulate dt into the runtime clock. Pressed doesn't actually use
	// it for its FSM but we keep the field consistent so the same
	// TriggerRuntimeState can be inspected uniformly.
	p_runtime.elapsed += p_delta_t;

	if (p_event_valid && p_pressed) {
		// Rising edge of a press event.
		if (p_runtime.state == STATE_NONE) {
			p_runtime.state = STATE_TRIGGERED;
			p_runtime.hold_start_time = p_runtime.elapsed;
			return RESULT_STARTED;
		}
		// Repeat press event while still in TRIGGERED (e.g. key repeat
		// in raw event streams): report TRIGGERED again.
		if (p_runtime.state == STATE_TRIGGERED) {
			return RESULT_TRIGGERED;
		}
		// State was ONGOING (shouldn't happen for Pressed, but defensively
		// promote to TRIGGERED and fire STARTED).
		if (p_runtime.state == STATE_ONGOING) {
			p_runtime.state = STATE_TRIGGERED;
			return RESULT_TRIGGERED;
		}
		return RESULT_NONE;
	}

	if (p_event_valid && !p_pressed) {
		// Release event: complete the lifecycle.
		if (p_runtime.state == STATE_TRIGGERED) {
			p_runtime.state = STATE_NONE;
			p_runtime.hold_start_time = -1.0;
			p_runtime.was_active = false;
			return RESULT_COMPLETED;
		}
		// Release while idle: nothing to complete.
		return RESULT_NONE;
	}

	// Tick (event_valid=false). Emit ONGOING while still in TRIGGERED
	// so listeners that bind to ONGOING see the held state.
	if (p_runtime.state == STATE_TRIGGERED) {
		return RESULT_ONGOING;
	}
	return RESULT_NONE;
}

String EITriggerPressed::get_trigger_name() const {
	return "Pressed";
}

void EITriggerPressed::_bind_methods() {
	// No config properties — Pressed has no parameters per spec.
}

} // namespace ei
