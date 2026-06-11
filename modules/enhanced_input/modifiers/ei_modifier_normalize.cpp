/**************************************************************************/
/* ei_modifier_normalize.cpp */
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

#include "ei_modifier_normalize.h"

#include "core/object/class_db.h"

namespace ei {

void EIModifierNormalize::set_min_length(float p_v) {
	_min_length = MAX(0.0f, p_v);
}

float EIModifierNormalize::get_min_length() const {
	return _min_length;
}

EIValue EIModifierNormalize::modify_value(const EIValue &p_input) const {
	switch (p_input.get_type()) {
		case EIValue::TYPE_BOOL: {
			return p_input;
		}
		case EIValue::TYPE_AXIS1D: {
			// Axis1D is already a scalar in [-1,1] by convention. Pass-through.
			return p_input;
		}
		case EIValue::TYPE_AXIS2D: {
			const Vector2 v = p_input.get_axis2d();
			const float len = v.length();
			if (len < _min_length) {
				return EIValue::make_axis2d(Vector2());
			}
			if (len ==0.0f) {
				return EIValue::make_axis2d(Vector2());
			}
			return EIValue::make_axis2d(v / len);
		}
		case EIValue::TYPE_AXIS3D: {
			const Vector3 v = p_input.get_axis3d();
			const float len = v.length();
			if (len < _min_length) {
				return EIValue::make_axis3d(Vector3());
			}
			if (len ==0.0f) {
				return EIValue::make_axis3d(Vector3());
			}
			return EIValue::make_axis3d(v / len);
		}
	}
	return p_input;
}

String EIModifierNormalize::get_modifier_name() const {
	return "Normalize";
}

void EIModifierNormalize::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_min_length", "length"), &EIModifierNormalize::set_min_length);
	ClassDB::bind_method(D_METHOD("get_min_length"), &EIModifierNormalize::get_min_length);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_length", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_min_length", "get_min_length");
}

} // namespace ei
