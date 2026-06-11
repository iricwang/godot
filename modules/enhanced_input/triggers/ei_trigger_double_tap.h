/**************************************************************************/
/*  ei_trigger_double_tap.h                                               */
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

// EITriggerDoubleTap per spec §4.4:
//   Two presses within `double_tap_time`, fire `Triggered` on the 2nd.
//
// Lifecycle:
//   * 1st press event (state NONE)             -> STARTED, state -> ONGOING
//   * 2nd press event within window            -> TRIGGERED, state -> TRIGGERED
//   * 2nd press event past window              -> treat as new 1st press: STARTED
//   * Tick while ONGOING, past window          -> CANCELED, state -> NONE
//   * Release event (state TRIGGERED)          -> COMPLETED, state -> NONE
//   * Release event (state ONGOING)            -> NONE; stay ONGOING, still
//                                                  waiting for the 2nd press
//
// We use the runtime fields:
//   * `consecutive_tap_count` to count presses (1 or 2 in practice)
//   * `last_tap_time` to know when the previous tap was

class EITriggerDoubleTap : public EITrigger {
	GDCLASS(EITriggerDoubleTap, EITrigger);

public:
	void set_double_tap_time(double p_v);
	double get_double_tap_time() const;

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
	// Max time (seconds) between the 1st and 2nd press to count as a
	// double-tap. 0.3s is a common gamepad threshold.
	double _double_tap_time = 0.3;
};

} // namespace ei
