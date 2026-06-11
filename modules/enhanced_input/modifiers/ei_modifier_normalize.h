/**************************************************************************/
/* ei_modifier_normalize.h */
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

// Normalize per spec §4.3: re-scale vector to length1 if non-zero.
// *1D: pass-through (already length1 in [-1,1] by convention).
// *2D/3D: divide by current length to produce unit vector.
// * Bool: pass-through.
//
// `min_length_to_normalize` (default0.0) suppresses output below a tiny
// threshold — avoids amplifying noise. UE calls this behavior "exponent".
// v1 keeps it as a simple threshold (length < min => zero).

class EIModifierNormalize : public EIModifier {
	GDCLASS(EIModifierNormalize, EIModifier);

public:
	void set_min_length(float p_v);
	float get_min_length() const;

	virtual EIValue modify_value(const EIValue &p_input) const override;
	virtual String get_modifier_name() const override;

protected:
	static void _bind_methods();

private:
	float _min_length =0.0f;
};

} // namespace ei
