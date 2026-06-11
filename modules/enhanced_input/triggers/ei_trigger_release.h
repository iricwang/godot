/**************************************************************************/
/*  ei_trigger_release.h                                                  */
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

// EITriggerRelease per spec §4.4:
//   Fire `Triggered` on the frame the value transitions from non-zero to zero.
//
// Implementation note: the spec says "value transitions from non-zero to
// zero" — i.e. it uses the EIValue's `is_zero()` predicate (spec §4.1).
// The trigger must therefore remember the previous value's zero-ness. The
// TriggerRuntimeState struct has a `was_active` field (added in P4) for
// exactly this purpose.
//
// Lifecycle:
//   * update_state() called, p_value.is_zero() == false
//       -> if was_active was true, return TRIGGERED (this is the rising
//          edge of "active" — i.e. the non-zero -> non-zero case; in
//          practice this is a no-op and we just keep state).
//          Actually: this is a "value became non-zero" event. The
//          Release trigger only fires on the FALLING edge (non-zero ->
//          zero). So we just record was_active=true and return NONE.
//   * update_state() called, p_value.is_zero() == true AND was_active was true
//       -> return TRIGGERED, was_active=false
//   * update_state() called, p_value.is_zero() == true AND was_active was false
//       -> return NONE, stay idle
//
// The `p_pressed` / `p_event_valid` parameters are unused: the trigger is
// value-driven, not event-driven. (A Release trigger typically doesn't
// need a separate "release" event — the value dropping to zero is
// sufficient.)

class EITriggerRelease : public EITrigger {
	GDCLASS(EITriggerRelease, EITrigger);

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
