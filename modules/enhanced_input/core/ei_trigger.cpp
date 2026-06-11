/**************************************************************************/
/*  ei_trigger.cpp                                                        */
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

#include "ei_trigger.h"

#include "core/object/class_db.h"

namespace ei {

// EITrigger is an abstract base. The concrete implementations
// (Pressed, Hold, Tap, DoubleTap, Release) live under triggers/.
//
// update_state() is a pure virtual; concrete subclasses in
// triggers/ei_trigger_*.cpp override it.
//
// Binding notes:
// * `update_state` is intentionally NOT bound to GDScript — it is a C++
//   virtual that returns an enum, but more importantly GDScript
//   subclasses would have no way to provide the deterministic
//   per-frame state machine the spec requires (review B8: triggers
//   SHALL be deterministic).
// * `get_trigger_name` IS bound, so the editor inspector can label
//   each subclass entry.
// * Subclasses inherit the GDCLASS chain via their own
//   GDCLASS(Macro, EITrigger) declaration.
void EITrigger::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_trigger_name"), &EITrigger::get_trigger_name);

	// Bind the enum constants so the editor inspector can render them.
	BIND_ENUM_CONSTANT(STATE_NONE);
	BIND_ENUM_CONSTANT(STATE_ONGOING);
	BIND_ENUM_CONSTANT(STATE_TRIGGERED);

	BIND_ENUM_CONSTANT(RESULT_NONE);
	BIND_ENUM_CONSTANT(RESULT_STARTED);
	BIND_ENUM_CONSTANT(RESULT_TRIGGERED);
	BIND_ENUM_CONSTANT(RESULT_ONGOING);
	BIND_ENUM_CONSTANT(RESULT_COMPLETED);
	BIND_ENUM_CONSTANT(RESULT_CANCELED);
}

} // namespace ei
