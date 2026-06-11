/**************************************************************************/
/* ei_modifier_negate.h */
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

#include "../core/ei_modifier.h"

namespace ei {

// Negate per spec §4.3:
//1D: x = -x
//2D/3D: per-axis bool flip (x, y, z)
// Bool: pass-through (negation of a boolean would invert meaning and is not
// a modifier the spec calls out)
//
// Default for all axes is false — pass-through — because the common case is
// "swap W/S so S gives -1.0 for move-back" and most axes are NOT negated.

class EIModifierNegate : public EIModifier {
	GDCLASS(EIModifierNegate, EIModifier);

public:
	void set_negate_x(bool p_v);
	bool get_negate_x() const;

	void set_negate_y(bool p_v);
	bool get_negate_y() const;

	void set_negate_z(bool p_v);
	bool get_negate_z() const;

	virtual EIValue modify_value(const EIValue &p_input) const override;
	virtual String get_modifier_name() const override;

protected:
	static void _bind_methods();

private:
	bool _negate_x =false;
	bool _negate_y =false;
	bool _negate_z =false;
};

} // namespace ei
