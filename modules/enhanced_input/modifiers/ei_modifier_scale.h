/**************************************************************************/
/* ei_modifier_scale.h */
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

// Scale per spec §4.3: multiplies value by per-axis scalar. Defaults all1.0.
// Bool: pass-through (scale of a boolean is meaningless).
//
// Named `scale_x/y/z` (not `x/y/z`) because `x` is too generic for inspector
// clarity when paired with other modifiers.

class EIModifierScale : public EIModifier {
	GDCLASS(EIModifierScale, EIModifier);

public:
	void set_scale_x(float p_v);
	float get_scale_x() const;

	void set_scale_y(float p_v);
	float get_scale_y() const;

	void set_scale_z(float p_v);
	float get_scale_z() const;

	virtual EIValue modify_value(const EIValue &p_input) const override;
	virtual String get_modifier_name() const override;

protected:
	static void _bind_methods();

private:
	float _scale_x =1.0f;
	float _scale_y =1.0f;
	float _scale_z =1.0f;
};

} // namespace ei
