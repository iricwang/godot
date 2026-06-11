/**************************************************************************/
/*  ei_trigger_pulse.h                                                   */
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

#include "../core/ei_trigger.h"

namespace ei {

// EITriggerPulse per spec §4.4:
//   Fire `Triggered` every `pulse_interval` seconds while held.
//
// Common use case: rapid-fire on a held key (auto-run, machinegun, etc.).
// On press, the first event fires STARTED; subsequent ticks fire
// TRIGGERED every `pulse_interval` seconds; on release, COMPLETED.
//
// Lifecycle:
//   * Press event  (state NONE)              -> STARTED, state -> ONGOING,
//                                                 last_fire_time = elapsed
//   * Tick         (state ONGOING,
//                   elapsed - last_fire_time >= pulse_interval)
//                                              -> TRIGGERED,
//                                                 last_fire_time = elapsed
//   * Tick         (state ONGOING, not yet due) -> NONE
//   * Tick         (state TRIGGERED)          -> ONGOING (still firing)
//   * Release      (state ONGOING/TRIGGERED)  -> COMPLETED
//
// Implementation note: we use the same `elapsed` clock as Hold/Tap. The
// pulse cadence is `elapsed - last_fire_time >= pulse_interval`. This
// keeps the trigger pure (no real-time clock needed) and lets the test
// drive time precisely via `p_delta_t`.

class EITriggerPulse : public EITrigger {
	GDCLASS(EITriggerPulse, EITrigger);

public:
	void set_pulse_interval(double p_v);
	double get_pulse_interval() const;

	virtual UpdateResult update_state(
			TriggerRuntimeState &p_runtime,
			const EIValue &p_value,
			double p_delta_t,
			bool p_event_valid,
			bool p_pressed) const override;

	virtual String get_trigger_name() const override;

protected:
	static void _bind_methods();

private:
	// Seconds between pulses while the trigger is held. UE default is
	// 1.0s; we use 0.5s as a more common gamepad-friendly value.
	double _pulse_interval = 0.5;
};

} // namespace ei
