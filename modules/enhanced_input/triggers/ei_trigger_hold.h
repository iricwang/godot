/**************************************************************************/
/*  ei_trigger_hold.h                                                     */
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

// EITriggerHold per spec §4.4:
//   After held `hold_time_threshold` seconds, fire
//   `Started→Triggered`, then `Ongoing` each tick, `Completed` on release.
//
// Spec scenario: EITriggerHold(0.3) on Space held for 0.5s:
//   * t=0.0  press event   -> STARTED     (state NONE -> ONGOING)
//   * t=0.0..0.3  ticks    -> NONE        (still in ONGOING, below threshold)
//   * t=0.3  tick          -> TRIGGERED   (state ONGOING -> TRIGGERED, hit threshold)
//   * t=0.4  tick          -> ONGOING     (state TRIGGERED, past threshold)
//   * t=0.5  tick          -> ONGOING
//   * t=?.?  release event -> COMPLETED   (state TRIGGERED -> NONE)
//
// If the user releases BEFORE the threshold fires, we emit CANCELED
// (state ONGOING -> NONE) instead of COMPLETED. This matches UE Enhanced
// Input's behavior and gives listeners a chance to distinguish
// "released before hold completed" from "hold completed normally".

class EITriggerHold : public EITrigger {
	GDCLASS(EITriggerHold, EITrigger);

public:
	void set_hold_time_threshold(double p_v);
	double get_hold_time_threshold() const;

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
	// Hold duration (seconds) before the trigger fires. UE default is
	// 0.4s; we use 0.3s as a more common gamepad threshold.
	double _hold_time_threshold = 0.3;
};

} // namespace ei
