/**************************************************************************/
/*  ei_trigger_tap.h                                                      */
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

// EITriggerTap per spec §4.4:
//   Fire `Triggered` on release, IF the press was released within
//   `tap_release_time`. If held longer, `Canceled`.
//
// Spec scenario: EITriggerTap(0.2) on Space held for 0.3s:
//   * t=0.0  press event   -> STARTED       (state NONE -> ONGOING)
//   * t=0.1  tick          -> ONGOING (or NONE)
//   * t=0.2  tick          -> CANCELED      (state ONGOING -> NONE; past window)
//   * t=0.3  release event -> NONE          (no Triggered ever fired)
//
// Success case: EITriggerTap(0.2) released at 0.1s:
//   * t=0.0  press event   -> STARTED       (state NONE -> ONGOING)
//   * t=0.1  release event -> TRIGGERED     (state ONGOING -> NONE; inside window)

class EITriggerTap : public EITrigger {
	GDCLASS(EITriggerTap, EITrigger);

public:
	void set_tap_release_time(double p_v);
	double get_tap_release_time() const;

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
	// Maximum hold time (seconds) for a press to still count as a tap.
	double _tap_release_time = 0.2;
};

} // namespace ei
