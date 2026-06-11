/**************************************************************************/
/*  ei_input_event_sampler.cpp                                           */
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

#include "ei_input_event_sampler.h"

// All InputEvent subtypes (InputEventKey, InputEventMouseButton,
// InputEventMouseMotion, InputEventJoypadButton, InputEventJoypadMotion)
// live in a single header in Godot 4.7.
#include "core/input/input_event.h"

namespace ei {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

// Typed zero for an action value type. Used when the event is null or
// of an unsupported subtype.
EIValue _zero_for(EIAction::ValueType p_t) {
	switch (p_t) {
		case EIAction::VALUE_TYPE_BOOL:
			return EIValue::make_bool(false);
		case EIAction::VALUE_TYPE_AXIS1D:
			return EIValue::make_axis1d(0.0f);
		case EIAction::VALUE_TYPE_AXIS2D:
			return EIValue::make_axis2d(Vector2());
		case EIAction::VALUE_TYPE_AXIS3D:
			return EIValue::make_axis3d(Vector3());
	}
	return EIValue::make_axis1d(0.0f);
}

// Map an EIAction::ValueType to the matching EIValue::Type. Used to
// promote the raw event value to the action's declared type.
EIValue::Type _type_of(EIAction::ValueType p_t) {
	switch (p_t) {
		case EIAction::VALUE_TYPE_BOOL:
			return EIValue::TYPE_BOOL;
		case EIAction::VALUE_TYPE_AXIS1D:
			return EIValue::TYPE_AXIS1D;
		case EIAction::VALUE_TYPE_AXIS2D:
			return EIValue::TYPE_AXIS2D;
		case EIAction::VALUE_TYPE_AXIS3D:
			return EIValue::TYPE_AXIS3D;
	}
	return EIValue::TYPE_AXIS1D;
}

} // namespace

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

EIValue EIInputEventSampler::sample(const Ref<InputEvent> &p_event, EIAction::ValueType p_value_type) {
	if (p_event.is_null()) {
		return _zero_for(p_value_type);
	}

	// Dispatch by event subtype. Order is significant: more specific
	// subtypes (mouse motion, joypad motion) come after the generic
	// button subtypes so a `Ref<>` cast to the more derived type picks
	// up the right one.
	EIValue raw;

	// Key event: pressed ? 1 : 0. `is_pressed()` is true on the down
	// event, false on the up event. Echo / repeat events also report
	// pressed=true — caller can dedupe via the dispatcher if needed.
	Ref<InputEventKey> key = p_event;
	if (key.is_valid()) {
		raw = EIValue::make_axis1d(key->is_pressed() ? 1.0f : 0.0f);
		return raw.with_type_promoted(_type_of(p_value_type));
	}

	// Mouse button: same semantics as key. Pressure is always 1.0 for
	// mouse buttons in Godot 4.x (no analog pressure), so a plain
	// pressed check suffices.
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		raw = EIValue::make_axis1d(mb->is_pressed() ? 1.0f : 0.0f);
		return raw.with_type_promoted(_type_of(p_value_type));
	}

	// Joypad button: pressure is 0..1; pass through directly. A
	// partially-pressed trigger returns 0..1.
	Ref<InputEventJoypadButton> jb = p_event;
	if (jb.is_valid()) {
		raw = EIValue::make_axis1d(jb->is_pressed() ? jb->get_pressure() : 0.0f);
		return raw.with_type_promoted(_type_of(p_value_type));
	}

	// Joypad motion: axis value is in [-1, 1]. Negative values are
	// legitimate (left stick left, right trigger released, etc.).
	Ref<InputEventJoypadMotion> jm = p_event;
	if (jm.is_valid()) {
		raw = EIValue::make_axis1d(jm->get_axis_value());
		return raw.with_type_promoted(_type_of(p_value_type));
	}

	// Mouse motion: relative motion since the last event. Naturally
	// a 2D vector. For Axis1D / Bool actions the relative motion
	// becomes 0 (it's a delta, not a magnitude).
	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		raw = EIValue::make_axis2d(mm->get_relative());
		return raw.with_type_promoted(_type_of(p_value_type));
	}

	// Unknown / unsupported event subtype. Future PRs can add more
	// (InputEventScreenTouch, InputEventScreenDrag, etc.) — see spec
	// §6.1 for the non-goals list.
	return _zero_for(p_value_type);
}

} // namespace ei
