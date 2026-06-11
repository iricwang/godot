/**************************************************************************/
/* ei_modifier_swizzle_axis.h */
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

// SwizzleAxis per spec §4.3: rearrange axes (e.g. (x,y,z) -> (y,z,x)).
// Configured by enum (AXIS_X / AXIS_Y / AXIS_Z) for each output axis.
//
//1D: pass-through (no second/third axis to swizzle from).
//2D: per-axis select. x_out = input[order_x], y_out = input[order_y]. The
// order_y enum's z value is treated as0 for2D.
//3D: full permutation.
// Bool: pass-through.

class EIModifierSwizzleAxis : public EIModifier {
	GDCLASS(EIModifierSwizzleAxis, EIModifier);

public:
	enum SourceAxis {
		SOURCE_AXIS_X =0,
		SOURCE_AXIS_Y =1,
		SOURCE_AXIS_Z =2,
	};

	void set_order_x(SourceAxis p_v);
	SourceAxis get_order_x() const;

	void set_order_y(SourceAxis p_v);
	SourceAxis get_order_y() const;

	void set_order_z(SourceAxis p_v);
	SourceAxis get_order_z() const;

	virtual EIValue modify_value(const EIValue &p_input) const override;
	virtual String get_modifier_name() const override;

protected:
	static void _bind_methods();

private:
	SourceAxis _order_x = SOURCE_AXIS_X;
	SourceAxis _order_y = SOURCE_AXIS_Y;
	SourceAxis _order_z = SOURCE_AXIS_Z;
};

} // namespace ei

VARIANT_ENUM_CAST(ei::EIModifierSwizzleAxis::SourceAxis);
