/**************************************************************************/
/* ei_modifier.cpp */
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

#include "ei_modifier.h"

#include "core/object/class_db.h"

namespace ei {

// EIModifier is an abstract base. The actual implementations (DeadZone,
// Negate, Scale, Normalize, SwizzleAxis) land in P3 under modifiers/.
//
// Binding notes:
// * `modify_value` is intentionally NOT bound to GDScript — it is a pure
// C++ virtual. GDScript subclasses would be possible but slow, and the
// spec explicitly says modifiers must be deterministic / thread-safe.
// * `get_modifier_name` IS bound, so the editor inspector can label each
// subclass entry.
// * Subclasses inherit the GDCLASS chain via their own GDCLASS(Macro, EIModifier).

void EIModifier::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_modifier_name"), &EIModifier::get_modifier_name);
}

} // namespace ei
