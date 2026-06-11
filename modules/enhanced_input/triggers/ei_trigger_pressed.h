/**************************************************************************/
/*  ei_trigger_pressed.h                                                  */
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

// EITriggerPressed per spec §4.4:
//   On `event_valid && p_pressed` fire `Started→Triggered→Completed`.
//
// Implementation note: the spec's "Started→Triggered→Completed" describes
// the EVENT SEQUENCE the action will see, not a single update_state() call.
// update_state() returns ONE UpdateResult per call. We split the lifecycle
// across calls as follows:
//   * First event with pressed=true  (state NONE → TRIGGERED):   STARTED
//   * Subsequent event with pressed=true  (state already TRIGGERED): TRIGGERED
//   * Tick while state==TRIGGERED:                                 ONGOING
//   * Event with pressed=false (state TRIGGERED → NONE):          COMPLETED
//   * Any other call:                                              NONE
//
// This gives a Pressed trigger a real lifecycle (Started → Triggered →
// Ongoing → Completed) that exercises the full FSM. For an action
// bound to a key, you'd press, see Started+Triggered on the rising
// edge, Ongoing every tick while held, and Completed on release.

class EITriggerPressed : public EITrigger {
	GDCLASS(EITriggerPressed, EITrigger);

public:
	virtual UpdateResult update_state(
			TriggerRuntimeState &p_runtime,
			const EIValue &p_value,
			double p_delta_t,
			bool p_event_valid,
			bool p_pressed) const override;

	virtual String get_trigger_name() const override;

protected:
	static void _bind_methods();
};

} // namespace ei
