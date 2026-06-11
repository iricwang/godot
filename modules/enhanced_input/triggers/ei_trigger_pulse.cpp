/**************************************************************************/
/*  ei_trigger_pulse.cpp                                                 */
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

#include "ei_trigger_pulse.h"

#include "core/object/class_db.h"

namespace ei {

void EITriggerPulse::set_pulse_interval(double p_v) {
	// Negative intervals are nonsensical; clamp to 0 (which would mean
	// "fire every tick", but in practice the editor should steer users
	// to EITriggerPressed for that).
	_pulse_interval = p_v < 0.0 ? 0.0 : p_v;
}

double EITriggerPulse::get_pulse_interval() const {
	return _pulse_interval;
}

EITrigger::UpdateResult EITriggerPulse::update_state(
		TriggerRuntimeState &p_runtime,
		const EIValue &p_value,
		double p_delta_t,
		bool p_event_valid,
		bool p_pressed) const {
	(void)p_value;

	// --- Press event ---------------------------------------------------
	if (p_event_valid && p_pressed) {
		if (p_runtime.state == STATE_NONE) {
			// Rising edge: arm the pulse clock. last_fire_time = elapsed
			// means "the first pulse is due at elapsed + pulse_interval".
			p_runtime.state = STATE_ONGOING;
			p_runtime.last_fire_time = p_runtime.elapsed;
			return RESULT_STARTED;
		}
		// Repeat press event while armed: no event.
		return RESULT_NONE;
	}

	// --- Release event -------------------------------------------------
	if (p_event_valid && !p_pressed) {
		if (p_runtime.state == STATE_ONGOING || p_runtime.state == STATE_TRIGGERED) {
			p_runtime.state = STATE_NONE;
			p_runtime.last_fire_time = -1.0;
			return RESULT_COMPLETED;
		}
		return RESULT_NONE;
	}

	// --- Tick (event_valid=false) --------------------------------------
	p_runtime.elapsed += p_delta_t;

	if (p_runtime.state == STATE_ONGOING || p_runtime.state == STATE_TRIGGERED) {
		// We're still held. Check whether a new pulse is due.
		const double since_fire = p_runtime.elapsed - p_runtime.last_fire_time;
		if (since_fire >= _pulse_interval) {
			// Time to fire (or re-fire). Mark the new last_fire_time,
			// promote to TRIGGERED, and report TRIGGERED. The next
			// tick (if no new pulse is due yet) will return ONGOING
			// so listeners on ONGOING can sustain between pulses.
			p_runtime.state = STATE_TRIGGERED;
			p_runtime.last_fire_time = p_runtime.elapsed;
			return RESULT_TRIGGERED;
		}
		// Not yet due. Sustain with ONGOING if we already fired on a
		// prior tick; otherwise stay quiet.
		return (p_runtime.state == STATE_TRIGGERED) ? RESULT_ONGOING : RESULT_NONE;
	}

	return RESULT_NONE;
}

String EITriggerPulse::get_trigger_name() const {
	return "Pulse";
}

void EITriggerPulse::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_pulse_interval", "seconds"), &EITriggerPulse::set_pulse_interval);
	ClassDB::bind_method(D_METHOD("get_pulse_interval"), &EITriggerPulse::get_pulse_interval);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pulse_interval", PROPERTY_HINT_RANGE, "0.0,5.0,0.01"), "set_pulse_interval", "get_pulse_interval");
}

} // namespace ei
