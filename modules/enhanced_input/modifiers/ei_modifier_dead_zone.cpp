/**************************************************************************/
/* ei_modifier_dead_zone.cpp */
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

#include "ei_modifier_dead_zone.h"

#include "core/object/class_db.h"

namespace ei {

void EIModifierDeadZone::set_min_deadzone(float p_v) {
	_min_deadzone = CLAMP(p_v,0.0f,1.0f);
}

float EIModifierDeadZone::get_min_deadzone() const {
	return _min_deadzone;
}

void EIModifierDeadZone::set_max_deadzone(float p_v) {
	_max_deadzone = CLAMP(p_v,0.0f,1.0f);
}

float EIModifierDeadZone::get_max_deadzone() const {
	return _max_deadzone;
}

// Pure-function dead-zone formula. Returns0 when |v| <= min, otherwise
// remaps [|min|, |max|] linearly to [0,1] preserving sign.
static float _apply_deadzone_scalar(float v, float min_d, float max_d) {
	const float sign = v <0.0f ? -1.0f :1.0f;
	const float mag = Math::abs(v);
	if (mag <= min_d) {
		return 0.0f;
	}
	if (max_d <= min_d) {
		// Degenerate: avoid division by zero. Treat as no deadzone.
		return sign * mag;
	}
	const float remapped = (mag - min_d) / (max_d - min_d);
	return sign * CLAMP(remapped,0.0f,1.0f);
}

EIValue EIModifierDeadZone::modify_value(const EIValue &p_input) const {
	switch (p_input.get_type()) {
		case EIValue::TYPE_BOOL: {
			// Pass-through — bool actions have no analog deadzone.
			return p_input;
		}
		case EIValue::TYPE_AXIS1D: {
			return EIValue::make_axis1d(_apply_deadzone_scalar(p_input.get_axis1d(), _min_deadzone, _max_deadzone));
		}
		case EIValue::TYPE_AXIS2D: {
			const Vector2 v = p_input.get_axis2d();
			// Radial deadzone: convert (x,y) to polar, apply1D formula to r.
			const float r = v.length();
			const float r_dz = _apply_deadzone_scalar(r, _min_deadzone, _max_deadzone);
			if (r_dz ==0.0f || r ==0.0f) {
				return EIValue::make_axis2d(Vector2());
			}
			// Preserve direction; scale back up to length r_dz (which is in [0,1]).
			const Vector2 dir = v / r;
			return EIValue::make_axis2d(dir * r_dz);
		}
		case EIValue::TYPE_AXIS3D: {
			// Per-axis1D deadzone (no radial3D in v1).
			const Vector3 v = p_input.get_axis3d();
			return EIValue::make_axis3d(Vector3(
					_apply_deadzone_scalar(v.x, _min_deadzone, _max_deadzone),
					_apply_deadzone_scalar(v.y, _min_deadzone, _max_deadzone),
					_apply_deadzone_scalar(v.z, _min_deadzone, _max_deadzone)));
		}
	}
	return p_input;
}

String EIModifierDeadZone::get_modifier_name() const {
	return "DeadZone";
}

void EIModifierDeadZone::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_min_deadzone", "deadzone"), &EIModifierDeadZone::set_min_deadzone);
	ClassDB::bind_method(D_METHOD("get_min_deadzone"), &EIModifierDeadZone::get_min_deadzone);
	ClassDB::bind_method(D_METHOD("set_max_deadzone", "deadzone"), &EIModifierDeadZone::set_max_deadzone);
	ClassDB::bind_method(D_METHOD("get_max_deadzone"), &EIModifierDeadZone::get_max_deadzone);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_deadzone", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_min_deadzone", "get_min_deadzone");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_deadzone", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_max_deadzone", "get_max_deadzone");
}

} // namespace ei
