/**************************************************************************/
/* ei_modifier.h */
/**************************************************************************/
/* This file is part of: */
/* GODOT ENGINE */
/* https://godotengine.org */
/**************************************************************************/
/* Copyright (c)2014-present Godot Engine contributors (see AUTHORS.md). */
/* */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the */
/* "Software"), to deal in the Software without restriction, including */
/* without limitation the rights to use, copy, modify, merge, publish, */
/* distribute, sublicense, and/or sell copies of the Software, and to */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions: */
/* */
/* The above copyright notice and this permission notice shall be */
/* included in all copies or substantial portions of the Software. */
/* */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
/**************************************************************************/

#pragma once

// Sibling include (same `core/` directory) so this header resolves the
// same way when included from test_main.cpp (which lacks our module's
// CPPATH) and from the in-module .cpp files.
#include "ei_value.h"
#include "core/io/resource.h"

namespace ei {

// Abstract base for value transformations applied to incoming EIValues.
//
// Per spec §4.3 (enhanced-input.md v0.2):
// * Every modifier is a pure function (no side effects, no state).
// * modify_value() is safe to call from any thread.
// * Modifiers that change value type are not allowed in v1.
//
// Concrete subclasses (DeadZone, Negate, Scale, Normalize, SwizzleAxis) are
// implemented in P3 under modifiers/.
//
// This class is registered with ClassDB so that:
//1. Subclasses can be saved as .tres resources alongside an EIAction.
//2. The EditorResourcePicker in EIAction's inspector can filter on it.
//3. TypedArray<EIModifier> serialization round-trips correctly.

class EIModifier : public Resource {
	GDCLASS(EIModifier, Resource);

public:
	// Pure function. Returns transformed value. Implementations must be
	// deterministic and MUST NOT change the value type.
	virtual EIValue modify_value(const EIValue &p_input) const =0;

	// Human-readable identifier for inspector display. Subclasses override.
	virtual String get_modifier_name() const =0;

protected:
	static void _bind_methods();
};

} // namespace ei
