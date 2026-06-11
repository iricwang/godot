/**************************************************************************/
/* ei_modifier_negate.cpp */
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

#include "ei_modifier_negate.h"

#include "core/object/class_db.h"

namespace ei {

void EIModifierNegate::set_negate_x(bool p_v) {
	_negate_x = p_v;
}

bool EIModifierNegate::get_negate_x() const {
	return _negate_x;
}

void EIModifierNegate::set_negate_y(bool p_v) {
	_negate_y = p_v;
}

bool EIModifierNegate::get_negate_y() const {
	return _negate_y;
}

void EIModifierNegate::set_negate_z(bool p_v) {
	_negate_z = p_v;
}

bool EIModifierNegate::get_negate_z() const {
	return _negate_z;
}

EIValue EIModifierNegate::modify_value(const EIValue &p_input) const {
	switch (p_input.get_type()) {
		case EIValue::TYPE_BOOL: {
			return p_input;
		}
		case EIValue::TYPE_AXIS1D: {
			const float v = p_input.get_axis1d();
			return EIValue::make_axis1d(_negate_x ? -v : v);
		}
		case EIValue::TYPE_AXIS2D: {
			const Vector2 v = p_input.get_axis2d();
			return EIValue::make_axis2d(Vector2(
					_negate_x ? -v.x : v.x,
					_negate_y ? -v.y : v.y));
		}
		case EIValue::TYPE_AXIS3D: {
			const Vector3 v = p_input.get_axis3d();
			return EIValue::make_axis3d(Vector3(
					_negate_x ? -v.x : v.x,
					_negate_y ? -v.y : v.y,
					_negate_z ? -v.z : v.z));
		}
	}
	return p_input;
}

String EIModifierNegate::get_modifier_name() const {
	return "Negate";
}

void EIModifierNegate::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_negate_x", "enabled"), &EIModifierNegate::set_negate_x);
	ClassDB::bind_method(D_METHOD("get_negate_x"), &EIModifierNegate::get_negate_x);
	ClassDB::bind_method(D_METHOD("set_negate_y", "enabled"), &EIModifierNegate::set_negate_y);
	ClassDB::bind_method(D_METHOD("get_negate_y"), &EIModifierNegate::get_negate_y);
	ClassDB::bind_method(D_METHOD("set_negate_z", "enabled"), &EIModifierNegate::set_negate_z);
	ClassDB::bind_method(D_METHOD("get_negate_z"), &EIModifierNegate::get_negate_z);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "negate_x"), "set_negate_x", "get_negate_x");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "negate_y"), "set_negate_y", "get_negate_y");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "negate_z"), "set_negate_z", "get_negate_z");
}

} // namespace ei
