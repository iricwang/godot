/**************************************************************************/
/*  ei_input_event_sampler.h                                             */
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

// Sibling include (same `core/` directory) — see ei_modifier.h for the
// rationale.
#include "ei_action.h"
#include "ei_value.h"

class InputEvent;

namespace ei {

// Bridges Godot's `InputEvent` stream into the per-action `EIValue`.
//
// Per spec §4.8:
// * For key events, `InputEventKey` is treated as Axis1D(1.0) on press,
//   Axis1D(0.0) on release. (UE Enhanced Input does the same.)
// * For joypad motion, the axis value is read directly.
// * For mouse motion, the relative motion becomes Axis2D.
//
// The sampler is a static helper with no internal state. The dispatcher
// (P5b) calls it once per matching mapping to produce the raw value
// before running it through the modifier chain.
//
// Edge cases handled:
// * Null event: returns a typed zero of `p_value_type`.
// * Unknown event subtype: typed zero (future PRs can add new types).
// * Type mismatch (e.g. Axis1D action bound to a mouse-motion event):
//   the raw type from the event is preserved and the action's modifier
//   pipeline / value type comparison is responsible for handling it.

class EIInputEventSampler {
public:
	// Sample a single event. `p_value_type` is the action's declared
	// type; the returned value is promoted to that type so callers
	// can treat it as a uniform EIValue (e.g. for is_zero() checks).
	static EIValue sample(const Ref<InputEvent> &p_event, EIAction::ValueType p_value_type);
};

} // namespace ei
